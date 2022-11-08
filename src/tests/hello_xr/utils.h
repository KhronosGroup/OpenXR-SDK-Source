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

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

const float PI = 3.1415927f;
const float TWO_PI = (float)6.28318530717958648;
const float PI_OVER_TWO = (float)1.57079632679489661923;
const float PI_OVER_FOUR = (float)0.785398163397448309616;
const float INV_PI = (float)0.318309886183790671538;
const float TWO_OVER_PI = (float)0.636619772367581343076;

const float EPSILON = 0.00001f;

const float DEG_TO_RAD = PI / 180.0f;
const float RAD_TO_DEG = 180.0f / PI;
const float ROOT_OF_HALF = 0.7071067690849304f;

#define rad2deg(a) ((a)*RAD_TO_DEG)
#define deg2rad(a) ((a)*DEG_TO_RAD)

namespace BVR
{
    
const glm::fquat no_rotation(1.0f, 0.0f, 0.0f, 0.0f);
const glm::fquat rotate_90_CCW_by_x(0.7071067690849304f, 0.7071067690849304f, 0.0f, 0.0f);
const glm::fquat rotate_180_CCW_about_y(0.0f, 1.0f, 0.0f, 0.0f);
const glm::fquat rotate_CW_45_rotation_about_x(0.9238795f, -0.3826834f, 0.0f, 0.0f);

const glm::fquat CCW_180_rotation_about_y = glm::fquat(0, 1, 0, 0);
const glm::fquat CCW_180_rotation_about_x = glm::fquat(0, 0, 1, 0);
const glm::fquat CCW_180_rotation_about_z = glm::fquat(0, 0, 0, 1);

const glm::fquat CCW_45_rotation_about_y = glm::fquat(0, 0.3826834f, 0, 0.9238795f);
const glm::fquat CW_45_rotation_about_y = glm::fquat(0, -0.3826834f, 0, 0.9238795f);

const glm::fquat CCW_90_rotation_about_y = glm::fquat(0, ROOT_OF_HALF, 0, ROOT_OF_HALF);
const glm::fquat CW_90_rotation_about_y = glm::fquat(0, -ROOT_OF_HALF, 0, ROOT_OF_HALF);

const glm::fquat CW_90_rotation_about_x = glm::fquat(-ROOT_OF_HALF, 0, 0, ROOT_OF_HALF);
const glm::fquat CCW_90_rotation_about_x = glm::fquat(ROOT_OF_HALF, 0, 0, ROOT_OF_HALF);

const glm::fquat CW_30deg_rotation_about_x = glm::fquat(-0.258819f, 0, 0, 0.9659258f);
const glm::fquat CCW_30deg_rotation_about_x = glm::fquat(0.258819f, 0, 0, 0.9659258f);

const glm::fquat CW_30deg_rotation_about_Y = glm::fquat(0.0f, 0.258819f, 0.0f, 0.9659258f);
const glm::fquat CCW_30deg_rotation_about_Y = glm::fquat(0.0f, -0.258819f, 0.0f, 0.9659258f);

const glm::fquat front_rotation = no_rotation;
const glm::fquat back_rotation = CCW_180_rotation_about_y;

const glm::fquat left_rotation = CCW_90_rotation_about_y;
const glm::fquat right_rotation = CW_90_rotation_about_y;

const glm::fquat floor_rotation = CW_90_rotation_about_x;
const glm::fquat ceiling_rotation = CCW_90_rotation_about_x;

const glm::fquat down_rotation = CW_90_rotation_about_x;
const glm::fquat up_rotation = CCW_90_rotation_about_x;


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

struct GLMPose
{
    GLMPose()
    {
    }
    
    glm::vec3 translation_ = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::fquat rotation_ = no_rotation;
    glm::vec3 scale_ = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 euler_angles_degrees_ = glm::vec3(0.0f, 0.0f, 0.0f);
    bool is_valid_  = true;

    void clear();
    glm::mat4 to_matrix() const;

    void update_rotation_from_euler();
};

GLMPose create_from_xr(const XrVector3f& position, const XrQuaternionf& rotation, const XrVector3f& scale);

}

#endif

