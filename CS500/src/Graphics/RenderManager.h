// ----------------------------------------------------------------------------
//	Copyright (C)DigiPen Institute of Technology.
//	Reproduction or disclosure of this file or its contents without the prior
//	written consent of DigiPen Institute of Technology is prohibited.
//
//	Purpose:		This file contains the declaration of the Render Manager class
//	Project:		cs550_j.zapata
//	Author:			Jon Zapata (j.zapata@digipen.edu)
// ----------------------------------------------------------------------------

#pragma once
#include "../Utilities/pch.hpp"
#include "GL/glew.h"
#include "../Utilities/Singleton.h"
#include "Shader.h"
#include "Window.h"
#include "Camera.h"

struct BlackHole;

struct CubeMap
{
	void CreateCubemap(const std::string& _dir);
	GLuint cubemapFBO[6]{};
	GLuint tex{};
	GLuint vao{};
};

enum class PolygonMode_t { Solid, Wireframe, PointCloud };
enum class DrawMode_t {Triangles, Points, Lines};

class RenderManager
{
	MAKE_SINGLETON(RenderManager)
public:
	void Initialize(int _width = 1280, int _height = 720);
	void Shutdown();
	void StartFrame() const;
	void EndFrame() const;

	void RenderAll();

	~RenderManager();

	const Camera& GetCamera() const { return camera; }
	Camera& GetCamera() { return camera; }
	const Window& GetWindow() const { return window; }

private:
	enum class ShaderType {SIMPLE, BLACK_HOLE, BLOOM_FIRST, BLOOM_SECOND};
	enum class CubemapType {SPACE, LAKE, PINK};

	void RenderScene();
	void BloomFirstPass();
	void BloomSecondPass();
	void CreateQuadTexture();
	void CreateSkybox();
	void RenderToQuadTexture();
	void CreateBuffers();
	void CreateShaders();
	void CreateHDRFrameBuffer();
	void CreateColorBuffers();
	void CreateBloomFrameBuffers();
	void CreateDiskTexture();
	void CreateBBTexture();
	void CreateNoiseTexture();
	void InitializeBH();
	void RenderBH();
	void RenderCubeMap();
	void Edit();
	void InitializeOpenGL() const;
	void CreateCubemaps();
	void UploadGenericUniforms();

	std::unordered_map<ShaderType, Shader*> shaders{};
	std::unordered_map<CubemapType, CubeMap*> cubemaps;
	CubemapType currentCubeMap = CubemapType::SPACE;
	Window window;
	Camera camera;
	BlackHole* BH;
	GLuint bloomFBO[2]{};
	GLuint bloomColorBuffers[2]{};
	GLuint colorBuffers[2]{};
	GLuint HDRFBO{};
};

#define GfxManager  RenderManager::Instance()