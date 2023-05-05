// ----------------------------------------------------------------------------
//	Copyright (C)DigiPen Institute of Technology.
//	Reproduction or disclosure of this file or its contents without the prior
//	written consent of DigiPen Institute of Technology is prohibited.
//
//	Purpose:		This file contains the declaration of the Camera class
//	Project:		cs550_j.zapata
//	Author:			Jon Zapata (j.zapata@digipen.edu)
// ----------------------------------------------------------------------------

#pragma once

#include <glm/glm.hpp>

class Camera
{
    public:
    void Initialize();
    void SetPosition(const glm::vec3 & newPos);
    void SetTarget(const glm::vec3 & newTarget);
    void SetProjection(float FOV, const glm::vec2 & Dimensions, float near, float far);
    void Update();

    glm::mat4 GetMtx() const;
    glm::mat4 GetProj() const { return mProjection; }
    glm::mat4 GetViewMat() const { return mW2C;}
    glm::vec3 GetPosition() const;
    glm::vec3 GetView() const { return mView; }
    glm::vec3 GetUp() const { return mUp; }
    glm::vec3 GetRight() const { return mRight; }
    glm::vec3 GetTarget() const { return mTarget; }

    void SetSpeed(float _s) { speed = _s; }

private:
    glm::mat4 mProjection = glm::mat4();
    glm::mat4 mCameraMatrix = glm::mat4();
    glm::mat4 mW2C = glm::mat4();
    glm::vec3 mPosition = { 0, 1, 55 };
    glm::vec3 mTarget = { 0, 0, 0 };
    glm::vec3 mView = {0, 0, 1};
    glm::vec3 mUp = { 0, 1, 0 };
    glm::vec3 mRight = { 1, 0, 0 };
    glm::vec2 mMousePos = {0, 0};
    float speed = 0.3f;
    float theta = 0.0f;
    float phi = 0.2f;
    float rad = 20.0f;
};
