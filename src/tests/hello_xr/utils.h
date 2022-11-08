#ifndef XR_UTILS_H__
#define XR_UTILS_H__

#include <common/xr_linear.h>
#include <string>

#ifdef XR_USE_PLATFORM_WIN32
#pragma warning(disable : 4201)
#endif

#include "glm/glm.hpp"
#include "glm/common.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/transform.hpp"

namespace Utils
{

template<typename T> static inline T clamp(T v, T mn, T mx)
{
    return (v < mn) ? mn : (v > mx) ? mx : v;
}

XrMatrix4x4f convert_to_xr(const glm::mat4& input);
glm::mat4 convert_to_glm(const XrMatrix4x4f& input);

XrVector3f convert_to_xr(const glm::vec3& input);
glm::vec3 convert_to_glm(const XrVector3f& input);

XrQuaternionf convert_to_xr(const glm::fquat& input);
glm::fquat convert_to_glm(const XrQuaternionf& input);

glm::mat4 convert_to_rotation_matrix(const glm::fquat& rotation);

}

#endif

