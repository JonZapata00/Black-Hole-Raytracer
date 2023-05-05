// ----------------------------------------------------------------------------
//	Copyright (C)DigiPen Institute of Technology.
//	Reproduction or disclosure of this file or its contents without the prior
//	written consent of DigiPen Institute of Technology is prohibited.
//
//	Purpose:		This file contains the implementation of the Render Manager class
//	Project:		cs500_j.zapata
//	Author:			Jon Zapata (j.zapata@digipen.edu)
// ----------------------------------------------------------------------------

#include <chrono>
#include "GL/glew.h"
#include "../Utilities/pch.hpp"
#include "../ImGui/imgui.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../Utilities/stb_image.h"
#include "../Utilities/ImGuiManager.h"
#include "BlackHole.h"
#include "RenderManager.h"

namespace
{
	static const int EnvMapSize = 512;
	static unsigned bloomIterations = 10;
	static unsigned quadVAO = 0;
	static unsigned quadVBO;
	static unsigned skyboxVAO;
	static unsigned skyboxVBO;
	static float timeElapsed = 0.0f;
	static bool mbApplyLensing = true;
	static bool mbRenderDisk = true;
	static bool mbApplyBloom = true;
}

/**
 * Creates window, Initializes OpenGL, creates shaders and initializes ImGui context
 * @param _width - window width
 * @param _height - window height
*/
void RenderManager::Initialize(int _width, int _height)
{
	window.GenerateWindow("cs500_j.zapata", { _width, _height });

	InitializeOpenGL();

	camera.Initialize();
	camera.SetProjection(60.0f, { _width, _height }, 0.001f, 1000.0f);

	CreateShaders();
	CreateQuadTexture();
	CreateSkybox();
	InitializeBH();
	CreateDiskTexture();
	CreateBBTexture();
	CreateNoiseTexture();
	CreateCubemaps();
	CreateBuffers();

	ImGuiMgr.Initialize();
}

/**
 * Clears the resources
*/
void RenderManager::Shutdown()
{
	for (auto& c : cubemaps)
		delete c.second;
	delete BH->diskTexture;
	delete BH->bbTexture;
}

/**
 * Begins a new frame
*/
void RenderManager::StartFrame() const
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glBindFramebuffer(GL_FRAMEBUFFER, HDRFBO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	ImGuiMgr.StartFrame();
}

/**
 * Ends the frame
*/
void RenderManager::EndFrame() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	ImGuiMgr.EndFrame();
}

/**
 * Renders all the objects in the scene
*/
void RenderManager::RenderAll()
{
	RenderScene();
	BloomFirstPass();
	BloomSecondPass();
	Edit();
	ImGuiMgr.Render();
	window.Swap();
}

/**
 * Frees allocated memory
*/
RenderManager::~RenderManager()
{
	Shutdown();
	for (auto& s : shaders)
	{
		delete s.second;
		s.second = nullptr;
	}
	shaders.clear();
}

/**
 * Renders the scene
*/
void RenderManager::RenderScene()
{
	shaders[ShaderType::BLACK_HOLE]->Use();
	UploadGenericUniforms();
	RenderBH();
	RenderCubeMap();
}

/**
 * Performs the first Bloom pass, in which we use ping pong
 * to blur the scene
*/
static bool horizontal = true;
void RenderManager::BloomFirstPass()
{
	shaders[ShaderType::BLOOM_FIRST]->Use();
	//we need to set this because we previously set the disk texture to be active
	glActiveTexture(GL_TEXTURE0);
	for (unsigned int i = 0; i < bloomIterations; i++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[horizontal]);
		shaders[ShaderType::BLOOM_FIRST]->SetUniform("horizontal", horizontal);
		glBindTexture(GL_TEXTURE_2D, i == 0 ? colorBuffers[1] : bloomColorBuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
		RenderToQuadTexture();
		horizontal = !horizontal;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/**
 * Performs the second Bloom pass, in which we render the scene and its blurred
 * counterpart to a two different textures (that will then be blend)
*/
void RenderManager::BloomSecondPass()
{
	//clear the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	shaders[ShaderType::BLOOM_SECOND]->Use();
	
	//bind and use both textures: the scene and the blurred one
	//(the latter will be either the horizontally blurred or the vertically
	//blurred)
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, bloomColorBuffers[!horizontal]);

	shaders[ShaderType::BLOOM_SECOND]->SetUniform("bloom", mbApplyBloom);
	RenderToQuadTexture();
}

/**
 * Creates a Quad that will be used to render a texture onto.
*/
void RenderManager::CreateQuadTexture()
{
	float quadVertices[] = 
	{
		// positions        // texture Coords
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	};
	// setup plane VAO
	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

}

void RenderManager::CreateSkybox()
{
	float skyboxVertices[] = {
		// positions          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
}

/**
 * Renders to a quad
*/
void RenderManager::RenderToQuadTexture()
{
	
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}

/**
 * Creates all the necessary buffers for the rendering process
*/
void RenderManager::CreateBuffers()
{
	CreateHDRFrameBuffer();
	CreateColorBuffers();
	CreateBloomFrameBuffers();
}

/**
 * Creates all the shaders
*/
void RenderManager::CreateShaders()
{
	shaders[ShaderType::SIMPLE] = new Shader("Resources/shaders/color.vert", "Resources/shaders/color.frag");
	shaders[ShaderType::BLACK_HOLE] = new Shader("Resources/shaders/color.vert", "Resources/shaders/BlackHole.frag");
	shaders[ShaderType::BLOOM_FIRST] = new Shader("Resources/shaders/BloomFirstPass.vert", "Resources/shaders/BloomFirstPass.frag");
	shaders[ShaderType::BLOOM_SECOND] = new Shader("Resources/shaders/BloomSecondPass.vert", "Resources/shaders/BloomSecondPass.frag");
	shaders[ShaderType::BLACK_HOLE]->Use();
}

/**
 * Creates a High Dynamic Range Frame buffer. Bloom looks
 * nicer when used with HDR
*/
void RenderManager::CreateHDRFrameBuffer()
{
	glGenFramebuffers(1, &HDRFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, HDRFBO);
}

/**
 * Creates both color buffers, one containing the actual scene
 * and the other containing only the brightness
*/
void RenderManager::CreateColorBuffers()
{
	glGenTextures(2, colorBuffers);
	for (unsigned int i = 0; i < 2; i++)
	{
		glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window.GetWindowSize().x, window.GetWindowSize().y, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
	}
	unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, attachments);
}

/**
 * Generates both frame buffers for the Bloom effect. 
*/
void RenderManager::CreateBloomFrameBuffers()
{
	//Generate two frame buffers: horizontal and vertical,
	//with their corresponding textures
	glGenFramebuffers(2, bloomFBO);
	glGenTextures(2, bloomColorBuffers);
	for (unsigned int i = 0; i < 2; i++)
	{
		//generate and attach the textures as corresponds
		glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[i]);
		glBindTexture(GL_TEXTURE_2D, bloomColorBuffers[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window.GetWindowSize().x, window.GetWindowSize().y, 0, GL_RGBA, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomColorBuffers[i], 0);
	}

	//upload the respective uniforms
	shaders[ShaderType::BLOOM_FIRST]->Use();
	shaders[ShaderType::BLOOM_FIRST]->SetUniform("image", 0);
	shaders[ShaderType::BLOOM_SECOND]->Use();
	shaders[ShaderType::BLOOM_SECOND]->SetUniform("scene", 0);
	shaders[ShaderType::BLOOM_SECOND]->SetUniform("blurredScene", 1);
}

/**
 * Creates the accretion disk texture
*/
void RenderManager::CreateDiskTexture()
{
	BH->diskTexture = new Texture();
	BH->diskTexture->dir = "Resources/Textures/starless_disk.jpg";
	BH->diskTexture->CreateTexture();
	shaders[ShaderType::BLACK_HOLE]->Use();
	shaders[ShaderType::BLACK_HOLE]->SetUniform("diskTexture", 0);
}

/**
 * Creates the (pseudo) Black Body texture for the accretion disk
*/
void RenderManager::CreateBBTexture()
{
	BH->bbTexture = new Texture();
	BH->bbTexture->dir = "Resources/Textures/noise.png";
	BH->bbTexture->CreateTexture();
	shaders[ShaderType::BLACK_HOLE]->Use();
	shaders[ShaderType::BLACK_HOLE]->SetUniform("bbodyTexture", 1);
}

void RenderManager::CreateNoiseTexture()
{
	BH->noiseTexture = new Texture();
	BH->noiseTexture->dir = "Resources/Textures/noise.png";
	BH->noiseTexture->CreateTexture();
	shaders[ShaderType::BLACK_HOLE]->Use();
	shaders[ShaderType::BLACK_HOLE]->SetUniform("noiseTexture", 2);
}

/**
 * Initializes the Black Hole shader by uploading constant
*/
void RenderManager::InitializeBH()
{
	BH = new BlackHole();
	float aspectRatio = (float)window.GetWindowSize().x / (float)window.GetWindowSize().y;
	shaders[ShaderType::BLACK_HOLE]->SetUniform("halfWidth", (float)window.GetWindowSize().x / 2.0f);
	shaders[ShaderType::BLACK_HOLE]->SetUniform("halfHeight", (float)window.GetWindowSize().y / 2.0f);
	shaders[ShaderType::BLACK_HOLE]->SetUniform("aspectRatio", aspectRatio);
	shaders[ShaderType::BLACK_HOLE]->SetUniform("focalLength", 1.0f);

	shaders[ShaderType::BLACK_HOLE]->SetUniform("BHPos", glm::vec3(0.0f));
	shaders[ShaderType::BLACK_HOLE]->SetUniform("EHRad", BH->EHRad);
	shaders[ShaderType::BLACK_HOLE]->SetUniform("innerDiskRad", BH->innerDiskRad);
	shaders[ShaderType::BLACK_HOLE]->SetUniform("outerDiskRad", BH->outerDiskRad);
	shaders[ShaderType::BLACK_HOLE]->SetUniform("beamExponent", BH->beamExp);
	shaders[ShaderType::BLACK_HOLE]->SetUniform("applyLensing", mbApplyLensing);
	shaders[ShaderType::BLACK_HOLE]->SetUniform("renderDisk", mbRenderDisk);
}

/**
 * Renders the Black Hole (aka binds both its textures)
*/
void RenderManager::RenderBH()
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, BH->diskTexture->tex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, BH->bbTexture->tex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, BH->noiseTexture->tex);
}

/**
 * Renders the cube map
*/
void RenderManager::RenderCubeMap()
{
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glBindVertexArray(cubemaps[currentCubeMap]->vao);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemaps[currentCubeMap]->tex);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glCullFace(GL_BACK); // set depth function back to default
	glEnable(GL_CULL_FACE);
}

/**
 * Uses ImGui to edit BH variables
*/
void RenderManager::Edit()
{
	if (ImGui::Begin("Edit BH raytracer"))
	{
		ImGui::Checkbox("Apply Bloom", &mbApplyBloom);
		if(mbApplyBloom)
			ImGui::SliderInt("Bloom Iterations", (int*)&bloomIterations, 1, 50);

		shaders[ShaderType::BLACK_HOLE]->Use();
		//Black hole
		if (ImGui::Checkbox("Apply Lensing", &mbApplyLensing))
			shaders[ShaderType::BLACK_HOLE]->SetUniform("applyLensing", mbApplyLensing);
		if (ImGui::Checkbox("Render Disk", &mbRenderDisk))
			shaders[ShaderType::BLACK_HOLE]->SetUniform("renderDisk", mbRenderDisk);

		//Accretion Disk
		if (ImGui::SliderFloat("Inner Disk Radius", &BH->innerDiskRad, 2.0f, BH->outerDiskRad - 2.0f))
			shaders[ShaderType::BLACK_HOLE]->SetUniform("innerDiskRad", BH->innerDiskRad);
		if(ImGui::SliderFloat("Outer Disk Radius", &BH->outerDiskRad, 4.0f, 20.0f))
			shaders[ShaderType::BLACK_HOLE]->SetUniform("outerDiskRad", BH->outerDiskRad);
		if (ImGui::SliderFloat("Beam exponent", &BH->beamExp, -15.0f, 15.0f))
			shaders[ShaderType::BLACK_HOLE]->SetUniform("beamExponent", BH->beamExp);

		//Skybox
		if (ImGui::RadioButton("Space", currentCubeMap == CubemapType::SPACE))
			currentCubeMap = CubemapType::SPACE;

		ImGui::SameLine();
		if (ImGui::RadioButton("Lake", currentCubeMap == CubemapType::LAKE))
			currentCubeMap = CubemapType::LAKE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Cotton candy", currentCubeMap == CubemapType::PINK))
			currentCubeMap = CubemapType::PINK;
	}
	ImGui::End();
}

/**
 * Initializes OpenGL context
*/
void RenderManager::InitializeOpenGL() const
{
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
}

/**
 * Creates the Cubemap 
*/
void RenderManager::CreateCubemaps()
{
	cubemaps[CubemapType::SPACE] = new CubeMap();
	cubemaps[CubemapType::SPACE]->CreateCubemap("Resources/Cubemaps/Nebula");
	cubemaps[CubemapType::LAKE] = new CubeMap();
	cubemaps[CubemapType::LAKE]->CreateCubemap("Resources/Cubemaps/Lake");
	cubemaps[CubemapType::PINK] = new CubeMap();
	cubemaps[CubemapType::PINK]->CreateCubemap("Resources/Cubemaps/CottonCandy");

	shaders[ShaderType::BLACK_HOLE]->Use();
	shaders[ShaderType::BLACK_HOLE]->SetUniform("cubeMap", 3);
}

/**
 * Uploads camera variables to the shader
*/
void RenderManager::UploadGenericUniforms()
{
	shaders[ShaderType::BLACK_HOLE]->SetUniform("camPos", camera.GetPosition());
	shaders[ShaderType::BLACK_HOLE]->SetUniform("view", camera.GetView());
	shaders[ShaderType::BLACK_HOLE]->SetUniform("up", camera.GetUp());
	shaders[ShaderType::BLACK_HOLE]->SetUniform("right", camera.GetRight());

	camera.Update();
	auto view = glm::mat4(glm::mat3(camera.GetViewMat()));
	shaders[ShaderType::BLACK_HOLE]->SetUniform("uniform_mvp", camera.GetProj() * view);
	timeElapsed += 0.016f;
	shaders[ShaderType::BLACK_HOLE]->SetUniform("timeElapsed", timeElapsed / 2.0f);
}

/**
 * Creates texture data for OpenGL using stbi
*/
void Texture::CreateTexture()
{
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	// set the texture wrapping/filtering options (on the currently bound texture object)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// load and generate the texture
	int width, height, nrChannels;
	unsigned char* data = stbi_load(dir.c_str(), &width, &height, &nrChannels, 0);
	if (data)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	else
		std::cout << "Failed to load texture" << std::endl;

	stbi_image_free(data);
}

/**
 * Creates Cubemap data for OpenGL using stbi
*/
void CubeMap::CreateCubemap(const std::string& _dir)
{
	vao = skyboxVAO;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
	std::vector<std::string> faces{ "right", "left", "top", "bottom", "front", "back" };
	//generate 1 image per face
	int width, height, comp;
	for (GLuint i = 0; i < faces.size(); ++i)
	{
		auto path = _dir + "/" + faces[i] + ".png";
		unsigned char* data = stbi_load(path.c_str(), &width, &height, &comp, 0);
		if (data)
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
		else
			std::cout << "Cubemap texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

}
