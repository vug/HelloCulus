#include "assert.h"
#include <iomanip>
#include <iostream>

#include <glad/glad_wgl.h>
#include <glad/glad.h>
#include <GL/freeglut.h>
#include <Windows.h>

#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <Extras/OVR_Math.h>

struct OculusTextureBuffer
{
	ovrSession          Session;
	ovrTextureSwapChain ColorTextureChain;
	ovrTextureSwapChain DepthTextureChain;
	GLuint              fboId;
	OVR::Sizei               texSize;

	OculusTextureBuffer(ovrSession session, OVR::Sizei size, int sampleCount) :
		Session(session),
		ColorTextureChain(nullptr),
		DepthTextureChain(nullptr),
		fboId(0),
		texSize(0, 0)
	{
		assert(sampleCount <= 1); // The code doesn't currently handle MSAA textures.

		texSize = size;

		// This texture isn't necessarily going to be a rendertarget, but it usually is.
		assert(session); // No HMD? A little odd.

		ovrTextureSwapChainDesc desc = {};
		desc.Type = ovrTexture_2D;
		desc.ArraySize = 1;
		desc.Width = size.w;
		desc.Height = size.h;
		desc.MipLevels = 1;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.SampleCount = sampleCount;
		desc.StaticImage = ovrFalse;

		{
			ovrResult result = ovr_CreateTextureSwapChainGL(Session, &desc, &ColorTextureChain);

			int length = 0;
			ovr_GetTextureSwapChainLength(session, ColorTextureChain, &length);

			if (OVR_SUCCESS(result))
			{
				for (int i = 0; i < length; ++i)
				{
					GLuint chainTexId;
					ovr_GetTextureSwapChainBufferGL(Session, ColorTextureChain, i, &chainTexId);
					glBindTexture(GL_TEXTURE_2D, chainTexId);

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
			}
		}

		desc.Format = OVR_FORMAT_D32_FLOAT;

		{
			ovrResult result = ovr_CreateTextureSwapChainGL(Session, &desc, &DepthTextureChain);

			int length = 0;
			ovr_GetTextureSwapChainLength(session, DepthTextureChain, &length);

			if (OVR_SUCCESS(result))
			{
				for (int i = 0; i < length; ++i)
				{
					GLuint chainTexId;
					ovr_GetTextureSwapChainBufferGL(Session, DepthTextureChain, i, &chainTexId);
					glBindTexture(GL_TEXTURE_2D, chainTexId);

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				}
			}
		}

		glGenFramebuffers(1, &fboId);
	}

	~OculusTextureBuffer()
	{
		if (ColorTextureChain)
		{
			ovr_DestroyTextureSwapChain(Session, ColorTextureChain);
			ColorTextureChain = nullptr;
		}
		if (DepthTextureChain)
		{
			ovr_DestroyTextureSwapChain(Session, DepthTextureChain);
			DepthTextureChain = nullptr;
		}
		if (fboId)
		{
			glDeleteFramebuffers(1, &fboId);
			fboId = 0;
		}
	}

	OVR::Sizei GetSize() const
	{
		return texSize;
	}

	void SetAndClearRenderSurface()
	{
		GLuint curColorTexId;
		GLuint curDepthTexId;
		{
			int curIndex;
			ovr_GetTextureSwapChainCurrentIndex(Session, ColorTextureChain, &curIndex);
			ovr_GetTextureSwapChainBufferGL(Session, ColorTextureChain, curIndex, &curColorTexId);
		}
		{
			int curIndex;
			ovr_GetTextureSwapChainCurrentIndex(Session, DepthTextureChain, &curIndex);
			ovr_GetTextureSwapChainBufferGL(Session, DepthTextureChain, curIndex, &curDepthTexId);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, fboId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curColorTexId, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, curDepthTexId, 0);

		glViewport(0, 0, texSize.w, texSize.h);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_FRAMEBUFFER_SRGB);
	}

	void UnsetRenderSurface()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fboId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
	}

	void Commit()
	{
		ovr_CommitTextureSwapChain(Session, ColorTextureChain);
		ovr_CommitTextureSwapChain(Session, DepthTextureChain);
	}
};

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
ovrPosef hmdToEyeViewPose[2];
ovrTextureSwapChain textureSwapChain = 0;
GLuint fboId;
ovrSizei bufferSize;
OculusTextureBuffer* eyeRenderTexture[2] = { nullptr, nullptr };
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
	//glClear(GL_COLOR_BUFFER_BIT);
	//glBegin(GL_TRIANGLES);
	//	glVertex3f(-.5, -.5, .0); glVertex3f(.5, .0, .0); glVertex3f(.0, .5, .0);
	//glEnd();
	// Above render flickers. Probably need proper sync with Rift render
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

			glClear(GL_COLOR_BUFFER_BIT);
			glBegin(GL_TRIANGLES);
			glVertex3f(v1.x, v1.y, v1.z);
			glVertex3f(v2.x, v2.y, v2.z);
			glVertex3f(v3.x, v3.y, v3.z);
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

	// Make eye render buffers
	for (int eye = 0; eye < 2; ++eye)
	{
		ovrSizei idealTextureSize = ovr_GetFovTextureSize(session, ovrEyeType(eye), hmdDesc.DefaultEyeFov[eye], 1);
		eyeRenderTexture[eye] = new OculusTextureBuffer(session, idealTextureSize, 1);
		if (!eyeRenderTexture[eye]->ColorTextureChain || !eyeRenderTexture[eye]->DepthTextureChain) { return 0; }
	}

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
	for (int eye = 0; eye < 2; ++eye) {
		delete eyeRenderTexture[eye];
	}
	ovr_Destroy(session);
	ovr_Shutdown();
	std::cout << "Bye, Rift!" << std::endl;
	return 0;
}