// THIRD PARTY INCLUDES
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <glm/glm.hpp>

// STD INCLUDES
#include <cstdlib>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

// SOURCE INCLUDES
#include <object.hpp>
#include <assetLoader.hpp>

// CONSTANTS
constexpr const char* WINDOW_TITLE = "Project-Infinite";
constexpr uint DEFAULT_WINDOW_WIDTH = 800;
constexpr uint DEFAULT_WINDOW_HEIGHT = 600;

uint loadShader(const char* path, GLenum type) {
	std::string src = assetLoader::loadTextFile(path);
	if (src.empty()) return 0;

	uint shader = glCreateShader(type);
	const char* cstr = src.c_str();
	glShaderSource(shader, 1, &cstr, nullptr);
	glCompileShader(shader);

	int ok = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		int len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		std::string log(len, '\0');
		glGetShaderInfoLog(shader, len, nullptr, log.data());
		std::cerr << "Shader error (" << path << "):\n" << log << "\n";
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

int main() {
	// INIT SDL & GL, CONFIG, CREATE WINDOW
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
		return EXIT_FAILURE;
	}
	
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif

	SDL_Window* window = SDL_CreateWindow(WINDOW_TITLE, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!window) {
		std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
		SDL_Quit();
		return EXIT_FAILURE;
	}

	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (!context) {
		std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << "\n";
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}

	if (gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress) == 0) {
		std::cerr << "Failed to initialize GLAD\n";
		SDL_GL_DestroyContext(context);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}

	// GL CONFIG
	glViewport(0, 0, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// SHADERS
	uint shaderProgram2D, shaderProgram3D;
	{
		uint v, f;

		// 2D
		shaderProgram2D = glCreateProgram();
		v = loadShader("assets/shaders/vertex2d.vert", GL_VERTEX_SHADER);
		glAttachShader(shaderProgram2D, v);
		f = loadShader("assets/shaders/fragment2d.frag", GL_FRAGMENT_SHADER);
		glAttachShader(shaderProgram2D, f);
		glLinkProgram(shaderProgram2D);
		glDeleteShader(v);
		glDeleteShader(f);

		// 3D
		shaderProgram3D = glCreateProgram();
		v = loadShader("assets/shaders/vertex3d.vert", GL_VERTEX_SHADER);
		glAttachShader(shaderProgram3D, v);
		f = loadShader("assets/shaders/fragment3d.frag", GL_FRAGMENT_SHADER);
		glAttachShader(shaderProgram3D, f);
		glLinkProgram(shaderProgram3D);
		glDeleteShader(v);
		glDeleteShader(f);
	}

	// VARIABLES
	glm::vec4 backgroundColor = {0.12f, 0.16f, 0.22f, 1.0f};
	bool running = true;
	SDL_Event event;
	std::vector<Object*> windowObjects;
	Camera* camera = new Camera();

	Object3D* cube = new Object3D({
    -0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f, 0.5f, -0.5f,
    -0.5f, 0.5f, -0.5f,
    -0.5f, -0.5f, 0.5f,
    0.5f, -0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    -0.5f, 0.5f, 0.5f
	}, {
    4, 5, 6,
		4, 6, 7,
    0, 3, 2,
		0, 2, 1,
    1, 2, 6,
		1, 6, 5,
    3, 0, 4,
		3, 4, 7,
    2, 3, 7,
		2, 7, 6,
    0, 1, 5,
		0, 5, 4
	}, shaderProgram2D);
	windowObjects.push_back(cube);
	cube->position = {0, 5, 0};

	// RENDER LOOP
	while (running) {

		// EVENT HANDLING
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}
			else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
				glViewport(0, 0, event.window.data1, event.window.data2);
			}
		}

		// CLEAR
		glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// CAMERA UPDATE
		camera->updateCamera(window);
		glm::mat4 vp3 = camera->projectionMatrix3D * camera->viewMatrix3D;
		glm::mat4 vp2 = camera->projectionMatrix2D;

		cube->rotation.z = SDL_GetTicks() / 1000.0f * 90;
		cube->rotation.x = SDL_GetTicks() / 1000.0f * 45;

		// GROUP OBJECTS
		std::vector<Object2D*> objects2D;
		std::vector<Object3D*> objects3DT;
		std::vector<Object3D*> objects3DO;
		for (Object* obj : windowObjects) {
			if (obj->dimensions == 2) {
				objects2D.push_back(dynamic_cast<Object2D*>(obj));
			} else if (obj->dimensions == 3) {
				(obj->colorRGBA.a == 1 ? objects3DO : objects3DT).push_back(dynamic_cast<Object3D*>(obj));
			}
		}
		sort(objects3DT.begin(), objects3DT.end(), [&camera](const Object3D* a, const Object3D* b) {
			glm::vec3 da = a->position - camera->position;
			glm::vec3 db = b->position - camera->position;
			return glm::dot(da, da) > glm::dot(db, db);
		});

		glUseProgram(shaderProgram3D);

		// 3D OPAQUE PASS
		glDisable(GL_BLEND);
		for (Object3D* obj : objects3DO) {
			obj->render(window, vp3);
		}

		// 3D TRANSPARENT PASS
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		for (Object3D* obj : objects3DT) {
			obj->render(window, vp3);
		}

		glUseProgram(shaderProgram2D);

		// 2D PASS
		glDisable(GL_DEPTH_TEST);
		for (Object2D* obj : objects2D) {
			obj->render(window, vp2);
		}
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);

		// SWAP
		SDL_GL_SwapWindow(window);
	}

	// QUIT
	for (Object* obj : windowObjects) {
    obj->deleteVAOVBOEBO();
    delete obj;
	}
	delete camera;
	glDeleteProgram(shaderProgram2D);
	glDeleteProgram(shaderProgram3D);
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return EXIT_SUCCESS;
}
