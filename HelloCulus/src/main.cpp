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

	ovrTrackerDesc trackerDesc = ovr_GetTrackerDesc(session, 0);

	ovrTimewarpProjectionDesc posTimewarpProjectionDesc = {};

	if (sessionStatus.ShouldQuit) glutLeaveMainLoop();
	if (sessionStatus.ShouldRecenter) ovr_RecenterTrackingOrigin(session);
	if (sessionStatus.IsVisible) {
		// Get next available index of the texture swap chain
		int currentIndex = 0;
		result = ovr_WaitToBeginFrame(session, frameIndex);

		// Render Scene to Eye Buffers
		result = ovr_BeginFrame(session, frameIndex);
		//OVR::Vector3f originPos(0.0f, 0.0f, 0.0f);
		OVR::Vector3f originPos(-0.3f, -1.1f, 1.4f);
		OVR::Matrix4f originRot = OVR::Matrix4f::Identity();
		for (int eye = 0; eye < 2; eye++) {
			eyeRenderTexture[eye]->SetAndClearRenderSurface();

			// Get view and projection matrices for the Rift camera
			OVR::Vector3f pos = originPos + originRot.Transform(EyeRenderPose[eye].Position);
			OVR::Vector3f pos = originPos + originRot.Transform(EyeRenderPose[eye].Position); // can scale Position to make camera move faster in VR world
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
			GLint uEyePos = glGetUniformLocation(prog, "ro");
			glProgramUniform3f(prog, uEyePos, pos.x, pos.y, pos.z);
			GLint uView = glGetUniformLocation(prog, "view");
			glProgramUniformMatrix4fv(prog, uView, 1, GL_FALSE, &(view.M[0][0]));
			GLint uProj = glGetUniformLocation(prog, "proj");
			glProgramUniformMatrix4fv(prog, uProj, 1, GL_FALSE, &(proj.M[0][0]));
			glProgramUniform1f(prog, glGetUniformLocation(prog, "frustFovH"), trackerDesc.FrustumHFovInRadians);
			glProgramUniform1f(prog, glGetUniformLocation(prog, "frustFovV"), trackerDesc.FrustumVFovInRadians);
			glProgramUniform1i(prog, glGetUniformLocation(prog, "eyeNo"), eye);

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
		unsigned int layerCount = 1;
		result = ovr_EndFrame(session, frameIndex, nullptr, &layers, layerCount);

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
		std::cout << "Idea Texture Size: (" << idealTextureSize.w << ", " << idealTextureSize.h << ")" << std::endl;
		eyeRenderTexture[eye] = new OculusTextureBuffer(session, idealTextureSize, 1);
		if (!eyeRenderTexture[eye]->ColorTextureChain || !eyeRenderTexture[eye]->DepthTextureChain) { return 0; }
	}
	mirrorBuffer = new OculusMirrorBuffer(session, mirrorSize);

	prog = glCreateProgram();
	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	// (2160, 1200), (1344, 1600)
	const static char* fsh2 = \
		"uniform float time = 0.0f;"
		"varying vec4 v;"
		"void main() {"
		"vec2 v = gl_FragCoord.xy / vec2(1344, 1600);"
		"v = mod(vec2(v.x + time, v.y), 1.0);"
		"gl_FragColor=vec4(floor(v.x * 10) / 10, floor(v.y * 10) / 10, 0.0, 1.0);"
		"}";
	const static char* fsh = \
		"uniform float time = 0.0f;"
		"uniform vec3 ro = vec3(0, 0, 1.0);"
		"uniform mat4 view = mat4(1.0);"
		"uniform mat4 proj = mat4(1.0);"
		"uniform float frustFovH = 1.7;"
		"uniform float frustFovV = 1.2;"
		"varying vec2 q;"
		"uniform int eyeNo = 0;"
		"float map(vec3 p) {"
		"    float d = distance(p, vec3(-1 + sin(time) * 2.0, 0, -5)) - 1.;"
		"    d = min(d, distance(p, vec3(2, 0, -3)) - 1.);"
		"    d = min(d, distance(p, vec3(-2, 0, -2)) - 1.);"
		"    d = min(d, p.y + 1.);"
		"    return d;"
		"}"
		"vec3 calcNormal(vec3 p) {"
		"    vec2 e = vec2(1.0, -1.0) * 0.0005;"
		"    return normalize("
		"        e.xyy * map(p + e.xyy) +"
		"        e.yyx * map(p + e.yyx) +"
		"        e.yxy * map(p + e.yxy) +"
		"        e.xxx * map(p + e.xxx));"
		"}"
		""
		"void main() {"
		"    vec2 res = vec2(1344, 1600);"
		"    vec2 q = (1.0 * gl_FragCoord.xy - 0.5 * res) / res;" // [-1, 1]
		"    vec3 rdFixed = normalize(vec3(q, -1));"

		"    vec3 forward = normalize( view * vec4(0, 0, -1, 1) );" // local forward
		"    vec3 u = normalize( cross(vec3(0, 1, 0), forward) );" // local X
		"    vec3 v = normalize( cross(forward, u) );" // local Y
		"    vec3 rd = normalize( -q.x * u + q.y * v + 1.5 * forward );" // make zoom a parameter

		//"    float z = 0.1 / tan(frustFovV * 0.1);"
		//"    vec3 rd = normalize((view * vec4(q.x, q.y, -z, 1.0)).xyz);"

		"    float correction = 1.0;" // eye distance correction, don't know why it works. 
		"    if (eyeNo == 0) { ro += u * correction; }"
		"    if (eyeNo == 1) { ro -= u * correction; }"

		"    float h, t = 1.;"
		"    for (int i = 0; i < 256; i++) {"
		"        h = map(ro + rd * t);"
		"        t += h;"
		"        if (h < 0.01) break;"
		"    }"
		"    if (h < 0.01) {"
		"        vec3 p = ro + rd * t;"
		"        vec3 normal = calcNormal(p);"
		"        vec3 light = vec3(0, 2, 0);"
		"        float dif = clamp(dot(normal, normalize(light - p)), 0., 1.);"
		"        dif *= 5. / dot(light - p, light - p);"
		"        gl_FragColor = vec4(vec3(pow(dif, 0.4545)), 1);"
		"    } else {"
		"        gl_FragColor = vec4(0, 0, 0, 1);"
		"    }"
		"}";
	glShaderSource(fs, 1, &fsh, 0);
	glCompileShader(fs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glUseProgram(prog);


	// Turn off vsync
	// wglSwapIntervalEXT(0); // throws "Access violation executing location" exception :-( glad problem?

	// FloorLevel will give tracking poses where the floor height is 0
	ovr_SetTrackingOriginType(session, ovrTrackingOrigin_FloorLevel); // ovrTrackingOrigin_EyeLevel

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