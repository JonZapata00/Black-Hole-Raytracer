#pragma once
#include "../Utilities/pch.hpp"
#include "GL/glew.h"

struct Texture
{
	std::string dir{ "Resources/Textures/starless_disk.jpg" };
	GLuint tex{};
	GLuint vao{};
	void CreateTexture();
};
struct BlackHole
{
	Texture* diskTexture;
	Texture* bbTexture;
	Texture* noiseTexture;
	float EHRad = 1.0f;
	float innerDiskRad = 2.0f;
	float outerDiskRad = 8.0f;
	float temperature = 10000.0f;
	float beamExp = 2.0f;
	float fallOffRate = 10.0f;
};