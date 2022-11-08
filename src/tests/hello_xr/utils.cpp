#include "utils.h"

namespace 
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
	rotation_ = no_rotation;
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

GLMPose create_from_xr(const XrVector3f& position, const XrQuaternionf& rotation, const XrVector3f& scale)
{
	GLMPose pose;
	pose.translation_ = convert_to_glm(position);
	pose.rotation_ = convert_to_glm(rotation);
	pose.scale_ = convert_to_glm(scale);
	return pose;
}

}





