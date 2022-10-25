#include <iostream>
#include <camera.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "VulkanTools.hpp"

int main() {

	vub::camera cam;

	std::string my_err = vub::tools::errorString(VK_SUCCESS);

	cam.position = glm::vec3(1.0);

	std::cout << "pos x " << cam.position.x << std::endl;

}