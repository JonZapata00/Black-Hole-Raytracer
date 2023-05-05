// ----------------------------------------------------------------------------
//	Copyright (C)DigiPen Institute of Technology.
//	Reproduction or disclosure of this file or its contents without the prior
//	written consent of DigiPen Institute of Technology is prohibited.
//
//	Purpose:		This file contains the implementation of the Camera class
//	Project:		cs500_j.zapata
//	Author:			Jon Zapata (j.zapata@digipen.edu)
// ----------------------------------------------------------------------------

#include "../Math/math.h"
#include "../Input/InputManager.h"
#include "RenderManager.h"
#include "Camera.h"

/**
  * Initializes camera by computing its vectors
*/
void Camera::Initialize()
{
    mView = glm::normalize(mTarget - mPosition);
    mRight = glm::normalize(glm::cross(mView, mUp));
    mUp = glm::normalize(glm::cross(mRight, mView));
}

/**
  * creates the window and initializes opengl, as well as creating the color shader
  * @param newPos - new position of the camera
*/
void Camera::SetPosition(const glm::vec3 & newPos) {mPosition = newPos;}

/**
  * sets a new target for the camera
  * @param newTarget - new target of the camera
*/
void Camera::SetTarget(const glm::vec3 & newTarget) {mTarget = newTarget;}

/**
  * sets perspective projection
  * @param FOV - field of view of the camera
  * @param DImensions - dimensions of the VP
  * @param near - near plane of the camera
  * @param far - far plane of the camera
*/
void Camera::SetProjection(float FOV, const glm::vec2 & Dimensions, float near, float  far)
{
    mProjection = glm::perspective(glm::radians(FOV), Dimensions.x / Dimensions.y, near, far);
}

/**
  * updates the camera matrix and movement
*/
void Camera::Update()
{
    rad = std::clamp(rad, 0.3f, 20.0f);
    
    if (KeyDown(Key::A)) theta += 0.02f;
    if (KeyDown(Key::D)) theta -= 0.02f;
    if (KeyDown(Key::S)) phi += 0.02f;
    if (KeyDown(Key::W)) phi -= 0.02f;
    if (KeyDown(Key::X)) rad += 0.08f;
    if (KeyDown(Key::Z)) rad -= 0.08f;
    
    mPosition.x = sinf(theta) * cosf(phi) * rad;
    mPosition.y = sinf(phi) * rad;
    mPosition.z = cosf(theta) * cosf(phi) * rad;
    
    phi = glm::clamp(phi, -glm::half_pi<float>() + 0.02f, glm::half_pi<float>() - 0.02f);
    theta -= 0.002f;
    mView = glm::normalize(mTarget - mPosition);
    mRight = glm::normalize(glm::cross(mView, { 0, 1, 0 }));
    mUp = -glm::normalize(glm::cross(mRight, mView));
    mW2C = glm::lookAt(mPosition, mTarget, { 0, 1, 0 });
    mCameraMatrix = mProjection * mW2C;
}

/**
  * retrieves the camera matrix
*/
glm::mat4 Camera::GetMtx() const { return mCameraMatrix; }

/**
  * gets the camera position
  * @return - camera position
 */
glm::vec3 Camera::GetPosition() const { return mPosition; }

