#pragma once
#include "assert.h"
#include <iostream>

#include <glad/glad.h>
#include <GL/freeglut.h>

#include <OVR_CAPI.h>
#include <OVR_CAPI_GL.h>
#include <Extras/OVR_Math.h>

struct OculusMirrorBuffer {
	ovrMirrorTexture mirrorTexture;
	GLuint fboId;
	OVR::Sizei texSize;

	OculusMirrorBuffer(const ovrSession& session, OVR::Sizei size) :
		mirrorTexture(nullptr),
		fboId(0),
		texSize(size) {
		ovrMirrorTextureDesc desc;
		memset(&desc, 0, sizeof(desc));
		desc.Width = texSize.w;
		desc.Height = texSize.h;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;

		ovrResult result = ovr_CreateMirrorTextureGL(session, &desc, &mirrorTexture);
		if (!OVR_SUCCESS(result)) { std::cout << "Unable to create mirror texture" << std::endl; exit(0); }
		GLuint texId;
		ovr_GetMirrorTextureBufferGL(session, mirrorTexture, &texId);
		glGenFramebuffers(1, &fboId);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, fboId);
		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
		glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	void render() {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, fboId);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, texSize.h, texSize.w, 0,
			0, 0, texSize.w, texSize.h,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glutSwapBuffers();
	}
};


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

