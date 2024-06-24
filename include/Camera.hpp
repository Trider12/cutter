#pragma once

#include "Glm.hpp"

class FlyCamera
{
public:
    FlyCamera() = default;

    void onRotate(float deltaX, float deltaY); // cursor delta pos in pixels
    void onMove(float axisX, float axisY, float axisZ);  // input values in local axes, non normalized
    void update(float delta);
    void lookAt(glm::vec3 target);
    glm::vec3 &getPosition();
    glm::mat4 getWorldMatrix() const;
    glm::mat4 getViewMatrix() const;

private:
    glm::vec3 position;
    glm::vec2 rotation; // radians
    glm::vec3 inputAxes;
    glm::vec2 cursorDelta;
    float speed = 5.f; // meters per second
    float angularSpeed = 0.1f; // radians per pixel per second
};