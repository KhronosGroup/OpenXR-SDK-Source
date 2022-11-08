#include "utils.h"

namespace Utils
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

}





