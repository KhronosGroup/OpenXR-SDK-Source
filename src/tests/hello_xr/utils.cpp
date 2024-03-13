#include "pch.h"

#if ENABLE_ADVANCED_FEATURES
#include "utils.h"

namespace BVR
{

XrMatrix4x4f convert_to_xr(const glm::mat4& input)
{
	XrMatrix4x4f output;
	memcpy(&output, &input, sizeof(output));
	return output;
}

glm::mat4 convert_to_glm(const XrMatrix4x4f& input)
{
	glm::mat4 output;
	memcpy(&output, &input, sizeof(output));
	return output;
}

XrVector3f convert_to_xr(const glm::vec3& input)
{
	XrVector3f output;
	memcpy(&output, &input, sizeof(output));
	return output;
}

glm::vec3 convert_to_glm(const XrVector3f& input)
{
	glm::vec3 output;
	output.x = input.x;
	output.y = input.y;
	output.z = input.z;
	return output;
}

XrQuaternionf convert_to_xr(const glm::fquat& input)
{
	XrQuaternionf output;
	output.x = input.x;
	output.y = input.y;
	output.z = input.z;
	output.w = input.w;
	return output;
}

glm::fquat convert_to_glm(const XrQuaternionf& input)
{
	glm::fquat output;
	output.x = input.x;
	output.y = input.y;
	output.z = input.z;
	output.w = input.w;
	return output;
}

glm::mat4 convert_to_rotation_matrix(const glm::fquat& rotation)
{
	glm::mat4 rotation_matrix = glm::mat4_cast(rotation);
	return rotation_matrix;
}

// GLMPose

void GLMPose::clear()
{
	translation_ = glm::vec3(0.0f, 0.0f, 0.0f);
	rotation_ = default_rotation;
	scale_ = glm::vec3(1.0f, 1.0f, 1.0f);
	euler_angles_degrees_ = glm::vec3(0.0f, 0.0f, 0.0f);
}

glm::mat4 GLMPose::to_matrix() const
{
	//glm::mat4 pre_translation_matrix = glm::translate(glm::mat4(1), pre_translation_);
	glm::mat4 translation_matrix = glm::translate(glm::mat4(1), translation_);
	glm::mat4 rotation_matrix = glm::mat4_cast(rotation_);
	glm::mat4 scale_matrix = glm::scale(scale_);

	//return translation_matrix * rotation_matrix * pre_translation_matrix * scale_matrix;
	return translation_matrix * rotation_matrix * scale_matrix;
}

void GLMPose::update_rotation_from_euler()
{
	glm::vec3 euler_angles_radians(deg2rad(euler_angles_degrees_.x), deg2rad(euler_angles_degrees_.y), deg2rad(euler_angles_degrees_.z));
	rotation_ = glm::fquat(euler_angles_radians);
}


void GLMPose::transform(const GLMPose& glm_pose)
{
	glm::vec3 rotated_translation = rotation_ * glm_pose.translation_;
	translation_ += rotated_translation;

	//translation_ += glm_pose.translation_;
	//rotation_ = glm::normalize(rotation_ * glm_pose.rotation_);
	//rotation_ = glm_pose.rotation_ * rotation_;

	rotation_ = glm::normalize(rotation_ * glm_pose.rotation_);

	//scale_ = scale_ * glm_pose.scale_;
	//euler_angles_degrees_ = glm_pose.euler_angles_degrees_;
}

GLMPose convert_to_glm(const XrVector3f& position, const XrQuaternionf& rotation, const XrVector3f& scale)
{
	GLMPose glm_pose;
	glm_pose.translation_ = convert_to_glm(position);
	glm_pose.rotation_ = convert_to_glm(rotation);
	glm_pose.scale_ = convert_to_glm(scale);
	return glm_pose;
}

GLMPose convert_to_glm(const XrPosef& xr_pose)
{
	GLMPose glm_pose;
	glm_pose.translation_ = convert_to_glm(xr_pose.position);
	glm_pose.rotation_ = convert_to_glm(xr_pose.orientation);
	return glm_pose;
}

XrPosef convert_to_xr(const GLMPose& glm_pose)
{
	// No scale
	XrPosef xr_pose;
	xr_pose.position = convert_to_xr(glm_pose.translation_);
	xr_pose.orientation = convert_to_xr(glm_pose.rotation_);
	return xr_pose;
}

}

#endif


