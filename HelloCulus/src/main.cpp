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
void glutDisplay(void) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glBegin(GL_TRIANGLES);
	glVertex3f(-0.5, -0.5, 0.0);
	glVertex3f(0.5, 0.0, 0.0);
	glVertex3f(0.0, 0.5, 0.0);
	glEnd();

	glutSwapBuffers();

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

	// The Oculus API allows the application to use either one shared texture or two separate textures for eye rendering. This example uses a single shared texture for simplicity, making it large enough to fit both eye renderings.
	ovrSizei recommendedTex0Size = ovr_GetFovTextureSize(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0], 1.0f);
	ovrSizei recommendedTex1Size = ovr_GetFovTextureSize(session, ovrEye_Left, hmdDesc.DefaultEyeFov[1], 1.0f);
	ovrSizei bufferSize;
	bufferSize.w = recommendedTex0Size.w + recommendedTex1Size.w;
	bufferSize.h = std::max(recommendedTex0Size.h, recommendedTex1Size.h);
	std::cout << "Render Texture Size: (" << bufferSize.w << ", " << bufferSize.h << ")" << std::endl;
	ovrTextureSwapChain textureSwapChain = 0;
	ovrTextureSwapChainDesc desc = {};
	desc.Type = ovrTexture_2D;
	desc.ArraySize = 1;
	desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.Width = bufferSize.w;
	desc.Height = bufferSize.h;
	desc.MipLevels = 1;
	desc.SampleCount = 1;
	desc.StaticImage = ovrFalse;
	result = ovr_CreateTextureSwapChainGL(session, &desc, &textureSwapChain);
	if (OVR_FAILURE(result)) { std::cout << "TextureSwapChain creation failed." << std::endl; return 0; }

	std::cout << "Press Q to quit." << std::endl;
	glutDisplayFunc(glutDisplay);
	glutIdleFunc(glutIdle);
	glutKeyboardFunc(glutKeyboard);
	glutMainLoop();

	ovr_Destroy(session);
	ovr_Shutdown();
	std::cout << "Bye, Rift!" << std::endl;
	return 0;
}