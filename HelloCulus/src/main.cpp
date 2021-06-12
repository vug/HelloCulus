#include <iomanip>
#include <iostream>

#include <glad/glad_wgl.h>
#include <glad/glad.h>
#include <GL/freeglut.h>
#include <Windows.h>

#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <Extras/OVR_Math.h>

void printHmdInfo(const ovrHmdDesc& desc) {
	std::cout << "Head Mounted Display Info" << std::endl;
	std::cout << "  Type: " << desc.Type << ". (14 is ovrHmd_CV1)" << std::endl;
	std::cout << "  Manufacturer: " << desc.Manufacturer << std::endl;
	std::cout << "  Firmware Version. Major: " << desc.FirmwareMajor << ", Minor: " << desc.FirmwareMinor << std::endl;
	std::cout << "  Resolution. Width: " << desc.Resolution.w << ", Height: " << desc.Resolution.h << std::endl;
	std::cout << "  Display Refresh Rate: " << desc.DisplayRefreshRate << std::endl;
	// AvailableHmdCaps, DefaultHmdCaps, AvailableTrackingCaps, DefaultTrackingCaps, DefaultEyeFov, MaxEyeFov
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
ovrLayerEyeFov layer;
ovrPosef hmdToEyeViewPose[2];
ovrTextureSwapChain textureSwapChain = 0;
GLuint fboId;
ovrSizei bufferSize;
long long frameIndex = 0;

void createRenderingTexture(const ovrHmdDesc& hmdDesc) {
	// The Oculus API allows the application to use either one shared texture or two separate textures for eye rendering. This example uses a single shared texture for simplicity, making it large enough to fit both eye renderings.
	ovrSizei recommendedTex0Size = ovr_GetFovTextureSize(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0], 1.0f);
	ovrSizei recommendedTex1Size = ovr_GetFovTextureSize(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1], 1.0f);
	bufferSize.w = recommendedTex0Size.w + recommendedTex1Size.w;
	bufferSize.h = std::max(recommendedTex0Size.h, recommendedTex1Size.h);
	std::cout << "Render Texture Size: (" << bufferSize.w << ", " << bufferSize.h << ")" << std::endl;
	ovrTextureSwapChainDesc desc = {};
	desc.Type = ovrTexture_2D;
	desc.ArraySize = 1;
	desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.Width = bufferSize.w;
	desc.Height = bufferSize.h;
	desc.MipLevels = 1;
	desc.SampleCount = 1;
	desc.StaticImage = ovrFalse;
	ovrResult result = ovr_CreateTextureSwapChainGL(session, &desc, &textureSwapChain);
	if (OVR_FAILURE(result)) { std::cout << "TextureSwapChain creation failed." << std::endl; exit(0); }

	int length = 0;
	ovr_GetTextureSwapChainLength(session, textureSwapChain, &length);
	std::cout << "Texture Swap Chain Length: " << length << std::endl;
	for (int i = 0; i < length; i++) {
		GLuint chainTexId;
		ovr_GetTextureSwapChainBufferGL(session, textureSwapChain, i, &chainTexId);
		glBindTexture(GL_TEXTURE_2D, chainTexId);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	glGenFramebuffers(1, &fboId);
}

void SetAndClearRenderSurface() {
	GLuint curColorTexId = 0;
	{
		int curIndex;
		ovr_GetTextureSwapChainCurrentIndex(session, textureSwapChain, &curIndex);
		ovr_GetTextureSwapChainBufferGL(session, textureSwapChain, curIndex, &curColorTexId);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, fboId);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curColorTexId, 0);

	glViewport(0, 0, bufferSize.w, bufferSize.h);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_FRAMEBUFFER_SRGB);
}

void UnsetRenderSurface()
{
	glBindFramebuffer(GL_FRAMEBUFFER, fboId);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
}

void glutDisplay(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBegin(GL_TRIANGLES);
	glVertex3f(-0.5, -0.5, 0.0);
	glVertex3f(0.5, 0.0, 0.0);
	glVertex3f(0.0, 0.5, 0.0);
	glEnd();

	glutSwapBuffers();

	ovrSessionStatus sessionStatus;
	ovrResult result;
	ovr_GetSessionStatus(session, &sessionStatus);

	// Call ovr_GetRenderDesc each frame to get the ovrEyeRenderDesc, as the returned values (e.g. HmdToEyePose) may change at runtime.
	ovrEyeRenderDesc eyeRenderDesc[2];
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
		ovr_GetTextureSwapChainCurrentIndex(session, textureSwapChain, &currentIndex);
		result = ovr_WaitToBeginFrame(session, frameIndex);

		// Clear and set up render-target.
		SetAndClearRenderSurface();

		// Render Scene to Eye Buffers
		result = ovr_BeginFrame(session, frameIndex);
		UnsetRenderSurface();
		// Commit the changes to the texture swap chain
		ovr_CommitTextureSwapChain(session, textureSwapChain);

		// Initialize our single full screen Fov layer.
		ovrLayerEyeFov layer; // ovrLayerEyeFovDepth
		layer.Header.Type = ovrLayerType_EyeFov;
		layer.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
		// layer.ProjectionDesc = posTimewarpProjectionDesc;
		layer.SensorSampleTime = sensorSampleTime;
		layer.ColorTexture[0] = textureSwapChain;
		layer.ColorTexture[1] = textureSwapChain;
		layer.Fov[0] = eyeRenderDesc[0].Fov;
		layer.Fov[1] = eyeRenderDesc[1].Fov;
		layer.Viewport[0] = OVR::Recti(0, 0, bufferSize.w / 2, bufferSize.h);
		layer.Viewport[1] = OVR::Recti(bufferSize.w / 2, 0, bufferSize.w / 2, bufferSize.h);

		// Submit frame with one layer we have.
		ovrLayerHeader* layers = &layer.Header;
		result = ovr_EndFrame(session, frameIndex, nullptr, &layers, 1);
		++frameIndex;
	ovrTrackingState ts = ovr_GetTrackingState(session, ovr_GetTimeInSeconds(), ovrTrue);
	printPositionAndOrientation(ts, timeStep);
	timeStep++;
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
	glutInitWindowSize(500, 500);
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
	createRenderingTexture(hmdDesc);

	std::cout << "Press Q to quit." << std::endl;
	glutDisplayFunc(glutDisplay);
	glutIdleFunc(glutIdle);
	glutKeyboardFunc(glutKeyboard);
	glutMainLoop();

	// Exit
	ovr_DestroyTextureSwapChain(session, textureSwapChain);
	textureSwapChain = nullptr;
	glDeleteFramebuffers(1, &fboId);
	fboId = 0;
	ovr_Destroy(session);
	ovr_Shutdown();
	std::cout << "Bye, Rift!" << std::endl;
	return 0;
}