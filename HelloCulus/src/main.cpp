#include <iomanip>
#include <iostream>

#include <glad/glad_wgl.h>
#include <glad/glad.h>
#include <GL/freeglut.h>
#include <Windows.h>

#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <Extras/OVR_Math.h>

#include "OculusBuffers.h"

void printHmdInfo(const ovrHmdDesc& desc) {
	std::cout << "Head Mounted Display Info" << std::endl;
	std::cout << "  Type: " << desc.Type << ". (14 is ovrHmd_CV1)" << std::endl;
	std::cout << "  Manufacturer: " << desc.Manufacturer << std::endl;
	std::cout << "  Firmware Version. Major: " << desc.FirmwareMajor << ", Minor: " << desc.FirmwareMinor << std::endl;
	std::cout << "  Resolution. Width: " << desc.Resolution.w << ", Height: " << desc.Resolution.h << std::endl;
	std::cout << "  Display Refresh Rate: " << desc.DisplayRefreshRate << std::endl;
}

void printPositionAndOrientation(ovrTrackingState ts, int timeStep) {
	std::cout << "\rtimeStep: " << std::setw(5) << timeStep << std::flush;
	if (ts.StatusFlags & ovrStatus_OrientationTracked) {
		ovrPosef pose = ts.HeadPose.ThePose;
		ovrVector3f pos = pose.Position;
		OVR::Quatf orientation = pose.Orientation;
		float yaw, pitch, roll;
		orientation.GetYawPitchRoll(&yaw, &pitch, &roll);
		std::cout
			<< std::showpos << std::fixed << std::setprecision(3)
			<< " position: (" << pos.x << ", " << pos.y << ", " << pos.z << ")"
			<< " yaw/pitch/roll: (" << yaw << ", " << pitch << ", " << roll << ")"
			<< std::flush;
	}
}

int timeStep = 0;
ovrSession session;
OculusTextureBuffer* eyeRenderTexture[2] = { nullptr, nullptr };
long long frameIndex = 0;
OVR::Sizei mirrorSize(600, 300);
OculusMirrorBuffer* mirrorBuffer;
GLuint prog;

void glutDisplay(void) {
	ovrSessionStatus sessionStatus;
	ovrResult result;
	ovr_GetSessionStatus(session, &sessionStatus);

	// Call ovr_GetRenderDesc each frame to get the ovrEyeRenderDesc, as the returned values (e.g. HmdToEyePose) may change at runtime.
	ovrEyeRenderDesc eyeRenderDesc[2];
	ovrPosef hmdToEyeViewPose[2];
	ovrHmdDesc hmdDesc2 = ovr_GetHmdDesc(session);
	eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc2.DefaultEyeFov[0]);
	eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc2.DefaultEyeFov[1]);
	hmdToEyeViewPose[0] = eyeRenderDesc[0].HmdToEyePose;
	hmdToEyeViewPose[1] = eyeRenderDesc[1].HmdToEyePose;

	// Get eye poses, feeding in correct IPD offset
	ovrPosef EyeRenderPose[2];
	ovrPosef HmdToEyePose[2] = { eyeRenderDesc[0].HmdToEyePose,
								 eyeRenderDesc[1].HmdToEyePose };
	double sensorSampleTime;    // sensorSampleTime is fed into the layer later
	ovr_GetEyePoses(session, frameIndex, ovrTrue, HmdToEyePose, EyeRenderPose, &sensorSampleTime);

	ovrTimewarpProjectionDesc posTimewarpProjectionDesc = {};

	if (sessionStatus.ShouldQuit) glutLeaveMainLoop();
	if (sessionStatus.ShouldRecenter) ovr_RecenterTrackingOrigin(session);
	if (sessionStatus.IsVisible) {
		// Get next available index of the texture swap chain
		int currentIndex = 0;
		result = ovr_WaitToBeginFrame(session, frameIndex);

		// Render Scene to Eye Buffers
		result = ovr_BeginFrame(session, frameIndex);
		OVR::Vector3f originPos(0.0f, 0.0f, 0.0f);
		OVR::Matrix4f originRot = OVR::Matrix4f::Identity();
		for (int eye = 0; eye < 2; eye++) {
			eyeRenderTexture[eye]->SetAndClearRenderSurface();

			// Get view and projection matrices for the Rift camera
			OVR::Vector3f pos = originPos + originRot.Transform(EyeRenderPose[eye].Position);
			OVR::Matrix4f rot = originRot * OVR::Matrix4f(EyeRenderPose[eye].Orientation);

			OVR::Vector3f finalUp = rot.Transform(OVR::Vector3f(0, 1, 0));
			OVR::Vector3f finalForward = rot.Transform(OVR::Vector3f(0, 0, -1));
			OVR::Matrix4f view = OVR::Matrix4f::LookAtRH(pos, pos + finalForward, finalUp);
			OVR::Matrix4f proj = ovrMatrix4f_Projection(hmdDesc2.DefaultEyeFov[eye], 0.2f, 1000.0f, ovrProjection_None);
			OVR::Matrix4f combined = proj * view;

			posTimewarpProjectionDesc = ovrTimewarpProjectionDesc_FromProjection(proj, ovrProjection_None);

			OVR::Vector3f v1(-1.5, -1.5, -1.0);
			OVR::Vector3f v2(1.5, 1.0, -1.0);
			OVR::Vector3f v3(1.0, 1.5, -1.0);
			v1 = combined.Transform(v1);
			v2 = combined.Transform(v2);
			v3 = combined.Transform(v3);

			GLint uTime = glGetUniformLocation(prog, "time");
			glProgramUniform1f(prog, uTime, sensorSampleTime);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glBegin(GL_QUADS);
			glVertex3f(-1, -1, 0);
			glVertex3f(1, -1, 0);
			glVertex3f(1, 1, 0);
			glVertex3f(-1, 1, 0);
			glEnd();

			eyeRenderTexture[eye]->UnsetRenderSurface();
			eyeRenderTexture[eye]->Commit();
		}

		ovrLayerEyeFovDepth ld = {};
		ld.Header.Type = ovrLayerType_EyeFovDepth;
		ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
		ld.ProjectionDesc = posTimewarpProjectionDesc;
		ld.SensorSampleTime = sensorSampleTime;

		for (int eye = 0; eye < 2; ++eye)
		{
			ld.ColorTexture[eye] = eyeRenderTexture[eye]->ColorTextureChain;
			ld.DepthTexture[eye] = eyeRenderTexture[eye]->DepthTextureChain;
			ld.Viewport[eye] = OVR::Recti(eyeRenderTexture[eye]->GetSize());
			ld.Fov[eye] = hmdDesc2.DefaultEyeFov[eye];
			ld.RenderPose[eye] = EyeRenderPose[eye];
		}

		// Submit frame with one layer we have.
		ovrLayerHeader* layers = &ld.Header;
		result = ovr_EndFrame(session, frameIndex, nullptr, &layers, 1);
		++frameIndex;
	}

	ovrTrackingState ts = ovr_GetTrackingState(session, ovr_GetTimeInSeconds(), ovrTrue);
	printPositionAndOrientation(ts, timeStep);
	timeStep++;

	mirrorBuffer->render();
}

void glutIdle() {
	glutPostRedisplay();
}

void glutKeyboard(unsigned char key, int x, int y) {
	if (key == 'q') {
		glutDestroyWindow(glutGetWindow());
		glutLeaveMainLoop();		
	}
}


int main(int argc, char** argv) {
	std::cout << "Hello, Rift!" << std::endl;
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(mirrorSize.w, mirrorSize.h);
	glutInitWindowPosition(0, 0);
	auto win = glutCreateWindow("Points");

	if (!gladLoadGL()) { std::cout << "Failed to initialize OpenGL context" << std::endl; return -1; }

	ovrResult result = ovr_Initialize(nullptr);
	if (OVR_FAILURE(result)) { std::cout << "Initialization failed with result code " << result << std::endl;  return result; }

	ovrGraphicsLuid luid;
	result = ovr_Create(&session, &luid);
	if (OVR_FAILURE(result)) { std::cout << "Creation failed with result code " << result << std::endl; ovr_Shutdown(); return result; }

	ovrHmdDesc hmdDesc = ovr_GetHmdDesc(session);
	printHmdInfo(hmdDesc);

	// Make eye render and mirror buffers
	for (int eye = 0; eye < 2; ++eye)
	{
		ovrSizei idealTextureSize = ovr_GetFovTextureSize(session, ovrEyeType(eye), hmdDesc.DefaultEyeFov[eye], 1);
		eyeRenderTexture[eye] = new OculusTextureBuffer(session, idealTextureSize, 1);
		if (!eyeRenderTexture[eye]->ColorTextureChain || !eyeRenderTexture[eye]->DepthTextureChain) { return 0; }
	}
	mirrorBuffer = new OculusMirrorBuffer(session, mirrorSize);

	prog = glCreateProgram();
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	// (2160, 1200), (1344, 1600)
	const static char* fsh = \
		"uniform float time = 0.0f;"
		"varying vec4 v;"
		"void main() {"
		"vec2 v = gl_FragCoord.xy / vec2(1344, 1600);"
		"v = mod(vec2(v.x + time, v.y), 1.0);"
		"gl_FragColor=vec4(floor(v.x * 10) / 10, floor(v.y * 10) / 10, 0.0, 1.0);"
		"}";
	glShaderSource(fs, 1, &fsh, 0);
	glCompileShader(fs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glUseProgram(prog);


	// Turn off vsync
	// wglSwapIntervalEXT(0); // throws "Access violation executing location" exception :-( glad problem?

	// FloorLevel will give tracking poses where the floor height is 0
	ovr_SetTrackingOriginType(session, ovrTrackingOrigin_FloorLevel);

	std::cout << "Press Q to quit." << std::endl;
	glutDisplayFunc(glutDisplay);
	glutIdleFunc(glutIdle);
	glutKeyboardFunc(glutKeyboard);
	glutMainLoop();

	// Exit
	for (int eye = 0; eye < 2; ++eye) {
		delete eyeRenderTexture[eye];
	}
	delete mirrorBuffer;
	ovr_Destroy(session);
	ovr_Shutdown();
	std::cout << "Bye, Rift!" << std::endl;
	return 0;
}