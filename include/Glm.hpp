#pragma once

#include <Utils.hpp>

#ifdef DEBUG
//#define GLM_FORCE_MESSAGES
#endif
#define GLM_FORCE_EXPLICIT_CTOR
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <glm/ext/vector_float3.hpp>

#include <glm/gtc/type_ptr.hpp>