#include "Camera.hpp"
#include "Utils.hpp"

void FlyCamera::onRotate(float deltaX, float deltaY)
{
    cursorDelta = glm::vec2(deltaX, deltaY);
}

void FlyCamera::onMove(float axisX, float axisY, float axisZ)
{
    inputAxes = glm::vec3(axisX, axisY, axisZ);
}

void FlyCamera::update(float delta)
{
    rotation -= glm::vec2(cursorDelta.y, cursorDelta.x) * angularSpeed * delta;
    rotation.x = glm::clamp(rotation.x, -glm::radians(89.f), glm::radians(89.f));

    if (glm::length2(inputAxes) > 0.f)
    {
        glm::mat4 worldMat = getWorldMatrix();
        glm::vec3 moveDir = glm::vec3(worldMat * glm::vec4(glm::normalize(inputAxes), 0.f));
        position += moveDir * speed * delta;
    }
}

void FlyCamera::lookAt(glm::vec3 target)
{
    glm::vec3 lookDir = glm::normalize(target - position);
    float x = lookDir.x;
    float y = lookDir.y;
    float z = lookDir.z;
    float yz = sqrtf(y * y + z * z);
    float zx = sqrtf(z * z + x * x);
    rotation.x = atan2(z / yz, y / yz);
    rotation.y = atan2(x / zx, z / zx);

    ASSERT(rotation.x >= -glm::radians(89.f) && rotation.x <= glm::radians(89.f));
}

glm::vec3 &FlyCamera::getPosition()
{
    return position;
}

glm::mat4 FlyCamera::getWorldMatrix() const
{
    glm::mat4 worldMat = glm::eulerAngleYX(rotation.y, rotation.x);
    worldMat[3] = glm::vec4(position, 1.f);
    return worldMat;
}

glm::mat4 FlyCamera::getViewMatrix() const
{
    glm::mat4 invRotationMat = glm::eulerAngleXY(-rotation.x, -rotation.y);
    glm::vec4 invTranslation = invRotationMat * glm::vec4(-position, 1.f);
    invRotationMat[3] = invTranslation;

    return invRotationMat;
}