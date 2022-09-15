#include <iostream>
#include <camera.hpp>

int main() {

	camera cam;
	cam.fov = 30;

	std::cout << "fov " << cam.fov << std::endl;

}