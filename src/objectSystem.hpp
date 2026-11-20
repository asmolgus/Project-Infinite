#pragma once

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace object {
	class Camera {
	public:
		glm::vec3 position = {0, 0, 0};
		glm::vec2 rotation = {0, 0};

		float fovY = 80;
		float near = 0.1f;
		float far = 1000.0f;

		glm::vec3 forward;
		glm::vec3 right;
		glm::vec3 up;
		glm::vec3 worldUp = {0, 0, 1};

		glm::mat4 projectionMatrix3D;
		glm::mat4 projectionMatrix2D;
		glm::mat4 viewMatrix3D;

		std::string id = "";
		std::vector<std::string> tags;

		void updateCamera(SDL_Window *window) {
			int width, height;
			SDL_GetWindowSizeInPixels(window, &width, &height);
			float aspect = (float)width / (float)height;

			forward = {glm::cos(rotation.y) * glm::sin(rotation.x), glm::cos(rotation.y) * glm::cos(rotation.x), glm::sin(rotation.y)};
			right = glm::normalize(glm::cross(forward, worldUp));
			up = glm::normalize(glm::cross(right, forward));

			projectionMatrix3D = glm::perspective(glm::radians(fovY), aspect, near, far);
			projectionMatrix2D = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
			viewMatrix3D = glm::lookAt(position, position + forward, worldUp);
		}

		Camera() {}
	};

	class Object {
	protected:
		int mvpLoc, colorLoc;
		GLenum drawType = GL_STATIC_DRAW;
		uint VAO, VBO, EBO;
		uint shaderProg;

	public:
		std::vector<float> vertices;
		std::vector<uint> indices;

		glm::vec3 position = {0, 0, 0};
		glm::vec3 scale = {1, 1, 1};
		glm::vec3 rotation = {0, 0, 0};

		glm::vec4 colorRGBA = {1, 1, 1, 1};

		uint dimensions = 0;

		std::string id = "";
		std::vector<std::string> tags;

		void setPosition(float x, float y, float z) { position = {x, y, z}; }
		std::tuple<float, float, float> getPosition() { return {position.x, position.y, position.z}; }

		void setColor(float r, float g, float b, float a) { colorRGBA = {r, g, b, a}; }
		std::tuple<float, float, float, float> getColor() { return {colorRGBA.r, colorRGBA.g, colorRGBA.b, colorRGBA.a}; }

		void setScale(float x, float y, float z) { scale = {x, y, z}; }
		std::tuple<float, float, float> getScale() { return {scale.x, scale.y, scale.z}; }

		void setRotation(float x, float y, float z) { rotation = {x, y, z}; }
		std::tuple<float, float, float> getRotation() { return {rotation.x, rotation.y, rotation.z}; }

		void deleteVAOVBOEBO() {
			glDeleteVertexArrays(1, &VAO);
			glDeleteBuffers(1, &VBO);
			glDeleteBuffers(1, &EBO);
		}

		void initVAOVBOEBO() {
			glGenVertexArrays(1, &VAO);
			glGenBuffers(1, &VBO);
			glGenBuffers(1, &EBO);

			updateVAOVBOEBO();
		}

		void updateVAOVBOEBO() {
			glBindVertexArray(VAO);

			glBindBuffer(GL_ARRAY_BUFFER, VBO);
			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint), indices.data(), GL_STATIC_DRAW);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
			glEnableVertexAttribArray(0);
			glBindVertexArray(0);
		}

		void setPrimitiveShaderLocs() {
			mvpLoc = glGetUniformLocation(shaderProg, "mvp");
			if (mvpLoc == -1) std::cerr << "MVP Loc returned -1!\n";

			colorLoc = glGetUniformLocation(shaderProg, "objectColor");
			if (colorLoc == -1) std::cerr << "Color Loc returned -1\n";
		}

		Object() {}

		Object(std::vector<float> _vertices, std::vector<uint> _indices, uint _dimensions, uint _shaderProg) {
			vertices = _vertices;
			indices = _indices;
			dimensions = _dimensions;
			shaderProg = _shaderProg;

			if (dimensions != 2 && dimensions != 3) {
				std::cerr << "Error on Object(): Cannot have an object with " << std::to_string(dimensions) << " dimensions!\n";
				throw std::invalid_argument("Invalid dimensions!");
			}

			initVAOVBOEBO();
			setPrimitiveShaderLocs();
			setDimensionSpecificShaderLocs();
		}

		virtual void setDimensionSpecificShaderLocs() {}

		virtual void render(SDL_Window *window, glm::mat4 vp) {}
	};

	class Object2D : public Object {
	public:
		void render(SDL_Window *window, glm::mat4 vp) override {
			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, position);
			model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
			model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
			model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
			model = glm::scale(model, scale);

			glm::mat4 mvp = vp * model;

			glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
			glUniform4fv(colorLoc, 1, glm::value_ptr(colorRGBA));

			glBindVertexArray(VAO);
			glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
		}

		Object2D(std::vector<float> _vertices, std::vector<uint> _indices, uint _shaderProg) : Object(_vertices, _indices, 2, _shaderProg) {}
	};

	class Object3D : public Object {
	public:
		void render(SDL_Window *window, glm::mat4 vp) override {
			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, position);
			model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
			model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
			model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
			model = glm::scale(model, scale);

			glm::mat4 mvp = vp * model;

			glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
			glUniform4fv(colorLoc, 1, glm::value_ptr(colorRGBA));

			glBindVertexArray(VAO);
			glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
		}

		Object3D(std::vector<float> _vertices, std::vector<uint> _indices, uint _shaderProg) : Object(_vertices, _indices, 3, _shaderProg) {}
	};

	template <typename T> std::optional<T> findObjectByID(std::vector<T> &all, std::string id = "") {
		using rawT = std::decay_t<T>;
		for (T x : all) {
			if constexpr (std::is_pointer_v<rawT>) {
				if (x && x->id == id) return x;
			} else {
				if (x.id == id) return x;
			}
		}
		if constexpr (std::is_pointer_v<rawT>) return nullptr;
	}

	template <typename T> std::vector<T> findObjectsByTag(std::vector<T> &all, std::string tag = "") {
		using rawT = std::decay_t<T>;
		std::vector<T> res;
		for (T x : all) {
			if constexpr (std::is_pointer_v<rawT>) {
				if (x == nullptr) continue;
				for (std::string y : x->tags) {
					if (y == tag) {
						res.push_back(x);
						break;
					}
				}
			} else {
				for (std::string y : x.tags) {
					if (y == tag) {
						res.push_back(x);
						break;
					}
				}
			}
		}
		return res;
	}
}
