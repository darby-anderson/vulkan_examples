/*
* 
* Basic Camera
*
* Partially based on Sascha Willem's camera example: https://github.com/SaschaWillems/Vulkan
*
*/

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vub {
class camera
{
	private:

		float fov;
		float zNear, zFar;

		void updateViewMatrix()
		{
			glm::mat4 rotM = glm::mat4(1.0f);
			glm::mat4 transM;

			rotM = glm::rotate(rotM, glm::radians(rotation.x * (flipY ? -1.0f : 1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
			rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
			rotM = glm::rotate(rotM, glm::radians(rotation.z ), glm::vec3(0.0f, 0.0f, 1.0f));

			glm::vec3 translation = rotation;
			if(flipY) {
				translation.y *= -1.0f;
			}
			transM = glm::translate(glm::mat4(1.0f), translation);

			matrices.view = rotM * transM;

			viewPos = glm::vec4(position, 1.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);
		}

	public:

		glm::vec3 rotation = glm::vec3();
		glm::vec3 position = glm::vec3();
		glm::vec4 viewPos = glm::vec4();

		bool flipY = true;

		struct 
		{
			glm::mat4 perspective;
			glm::mat4 view;
		} matrices;

		float getNearClip() {
			return zNear;
		}

		float getFarClip() {
			return zFar;
		}

		void setPerspective(float fovDegrees, float aspect, float zNear, float zFar) {
			this->fov = fovDegrees;
			this->zNear = zNear;
			this->zFar = zFar;
			matrices.perspective = glm::perspective(glm::radians(fovDegrees), aspect, zNear, zFar);

			if (flipY) {
				matrices.perspective[1][1] *= -1;
			}
		}

		void setPosition(glm::vec3 position)
		{
			this->position = position;
			updateViewMatrix();
		}

		void setRotation(glm::vec3 rotation)
		{
			this->rotation = rotation;
			updateViewMatrix();
		}

		void lookAt(glm::vec3 eye, glm::vec3 target)
		{
			matrices.view = glm::lookAt(eye, target, glm::vec3(0.0f, 0.0f, 1.0f));
		}

};
}

