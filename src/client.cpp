// THIRD PARTY INCLUDES
#include "sol/table.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_timer.h>
#include <enet/enet.h>
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <sol/optional_implementation.hpp>

// STD INCLUDES
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

// SOURCE INCLUDES
#include <assetLoader.hpp>
#include <luaIntegration.hpp>
#include <objectSystem.hpp>
#include <serverConnection.hpp>

// CONSTANTS
constexpr const char *WINDOW_TITLE = "Project-Infinite";
constexpr uint DEFAULT_WINDOW_WIDTH = 800;
constexpr uint DEFAULT_WINDOW_HEIGHT = 600;

uint loadShader(const char *path, GLenum type) {
	std::string src = assetLoader::loadTextFile(path);
	if (src.empty()) return 0;

	uint shader = glCreateShader(type);
	const char *cstr = src.c_str();
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
	// INIT ENET
	try {
		initENet();
	} catch (std::exception e) { return EXIT_FAILURE; }

	atexit(enet_deinitialize);

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

	SDL_Window *window = SDL_CreateWindow(WINDOW_TITLE, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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
	glm::vec4 backgroundColor = {1.0f, 1.0f, 1.0f, 1.0f};
	bool running = true;
	SDL_Event event;
	std::vector<object::Object *> windowObjects;
	object::Camera *camera = new object::Camera();
	double dt = 0.0;

	// CLIENT
	GameClient client;
	sol::table onRender;
	bool couldClientInit = client.init();
	running = couldClientInit;
	if (couldClientInit) {
		client.connect("127.0.0.1", 30000);

		// LUA BINDINGS
		client.luaState = luaIntegration::createState();

		onRender = client.luaState.create_named_table("onRender");

		client.luaState["ELAPSED_TIME"] = 0UL;

		client.luaState["createObject2D"] = [&windowObjects, &shaderProgram2D](std::vector<float> vertices, std::vector<uint> indices) -> object::Object2D * {
			object::Object2D *x = new object::Object2D(vertices, indices, shaderProgram2D);
			windowObjects.push_back(x);
			return x;
		};
		client.luaState["createObject3D"] = [&windowObjects, &shaderProgram3D](std::vector<float> vertices, std::vector<uint> indices) -> object::Object3D * {
			object::Object3D *x = new object::Object3D(vertices, indices, shaderProgram3D);
			windowObjects.push_back(x);
			return x;
		};
		client.luaState["getObjectByID"] = [&windowObjects](std::string id) -> std::optional<object::Object *> { return object::findObjectByID(windowObjects, id); };
		client.luaState["getObjectsByTag"] = [&windowObjects](std::string tag) -> std::vector<object::Object *> { return object::findObjectsByTag(windowObjects, tag); };
		client.luaState["destroyObject"] = [&windowObjects](object::Object *obj) -> void {
			auto loc = std::find(windowObjects.begin(), windowObjects.end(), obj);
			if (loc != windowObjects.end()) windowObjects.erase(loc);
			delete obj;
		};
		client.luaState["setBackgroundColor"] = [&backgroundColor](float r, float g, float b, float a) -> void { backgroundColor = {r, g, b, a}; };
		client.luaState["getBackgroundColor"] = [&backgroundColor]() -> std::tuple<float, float, float, float> { return {backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a}; };
		client.luaState["executeFile"] = [&client](std::string path) -> void {
			sol::protected_function_result res = client.luaState.do_file(path, sol::load_mode::text);
			if (!res.valid()) { std::cout << "File execution error: " << ((sol::error)res).what() << "\n"; }
		};

		client.luaState.new_usertype<object::Object>("Object", "id", &object::Object::id, "getPosition", &object::Object::getPosition, "setPosition", &object::Object::setPosition, "getColor", &object::Object::getColor, "setColor", &object::Object::setColor, "getScale", &object::Object::getScale, "setScale", &object::Object::setScale, "getRotation", &object::Object::getRotation, "setRotation", &object::Object::setRotation);
		client.luaState.new_usertype<object::Object3D>("Object3D", sol::base_classes, sol::bases<object::Object>());
		client.luaState.new_usertype<object::Object2D>("Object2D", sol::base_classes, sol::bases<object::Object>());
	}

	// RENDER LOOP
	while (running) {

		unsigned long int startTime = SDL_GetTicksNS();

		// EVENT HANDLING
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			} else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
				glViewport(0, 0, event.window.data1, event.window.data2);
			}
		}

		// CLEAR
		glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// CLIENT UPDATES & ONRENDER
		client.luaState["ELAPSED_TIME"] = SDL_GetTicks();
		client.poll();
		{
			sol::optional<sol::table> _hooks = client.luaState["onRender"];
			if (_hooks) {
				sol::table hooks = *_hooks;
				for (size_t i = 1; i <= hooks.size(); ++i) {
					sol::optional<sol::protected_function> fnOpt = hooks[i];
					if (!fnOpt) continue;

					sol::protected_function &fn = *fnOpt;
					sol::protected_function_result res = fn(dt);
					if (!res.valid()) std::cerr << "Lua onRender hook error (#" << i << "): " << ((sol::error)res).what() << "\n";
				}
			}
		}

		// CAMERA UPDATES
		camera->updateCamera(window);
		glm::mat4 vp3 = camera->projectionMatrix3D * camera->viewMatrix3D;
		glm::mat4 vp2 = camera->projectionMatrix2D;

		// GROUP OBJECTS
		std::vector<object::Object2D *> objects2D;
		std::vector<object::Object3D *> objects3DT;
		std::vector<object::Object3D *> objects3DO;
		for (object::Object *obj : windowObjects) {
			if (obj->dimensions == 2) {
				objects2D.push_back(reinterpret_cast<object::Object2D *>(obj));
			} else if (obj->dimensions == 3) {
				(obj->colorRGBA.a == 1 ? objects3DO : objects3DT).push_back(reinterpret_cast<object::Object3D *>(obj));
			}
		}
		sort(objects3DT.begin(), objects3DT.end(), [&camera](const object::Object3D *a, const object::Object3D *b) {
			glm::vec3 da = a->position - camera->position;
			glm::vec3 db = b->position - camera->position;
			return glm::dot(da, da) > glm::dot(db, db);
		});

		glUseProgram(shaderProgram3D);

		// 3D OPAQUE PASS
		glDisable(GL_BLEND);
		for (object::Object3D *obj : objects3DO) { obj->render(window, vp3); }

		// 3D TRANSPARENT PASS
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);
		for (object::Object3D *obj : objects3DT) { obj->render(window, vp3); }

		glUseProgram(shaderProgram2D);

		// 2D PASS
		glDisable(GL_DEPTH_TEST);
		for (object::Object2D *obj : objects2D) { obj->render(window, vp2); }
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);

		// SWAP
		SDL_GL_SwapWindow(window);

		dt = glm::clamp(static_cast<double>(SDL_GetTicksNS() - startTime) / 1000000000.0, 0.0, 0.1);
	}

	// QUIT
	for (object::Object *obj : windowObjects) {
		obj->deleteVAOVBOEBO();
		delete obj;
	}
	delete camera;
	glDeleteProgram(shaderProgram2D);
	glDeleteProgram(shaderProgram3D);
	SDL_GL_DestroyContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return (couldClientInit ? EXIT_SUCCESS : EXIT_FAILURE);
}
