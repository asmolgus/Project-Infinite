#include <enet/enet.h>
#include <iostream>
#include <string>

#include <luaIntegration.hpp>
#include <serverHosting.hpp>

constexpr uint PORT = 30000;
constexpr uint MAX_CLIENTS = 32;
constexpr uint CHANNELS = 2;

int main() {
	std::cout << "Initializing dedicated server...\n";

	try {
		initENet();
	} catch (std::exception e) { return EXIT_FAILURE; }
	atexit(enet_deinitialize);

	GameServer server;
	server.port = PORT;
	server.maxClients = MAX_CLIENTS;
	server.channels = CHANNELS;
	server.luaState = luaIntegration::createState();

	server.luaState["sendMessage"] = [&server](ENetPeer *peer, const std::string &message, uint channel = 0, bool reliable = true) -> void { server.sendMessage(peer, message, channel, reliable); };
	server.luaState["broadcastMessage"] = [&server](const std::string &message, uint channel = 0, bool reliable = true) -> void { server.broadcastMessage(message, channel, reliable); };
	server.luaState["sendLua"] = [&server](ENetPeer *peer, const std::string &message, uint channel = 0) -> void { server.sendLua(peer, message, channel); };
	server.luaState["broadcastLua"] = [&server](const std::string &message, uint channel = 0) -> void { server.broadcastLua(message, channel); };
	server.luaState["executeFile"] = [&server](const std::string &path) -> void { server.executeLuaFile(path); };

	if (!server.start()) {
		std::cerr << "Error starting server!\n";
		return EXIT_FAILURE;
	}

	std::string command;
	while (server.isRunning) {
		std::cout << "server> ";

		if (!std::getline(std::cin, command)) { break; }

		if (command == "exit" || command == "quit") {
			std::cout << "Shutting down console command loop\n";
			break;
		}

		if (command.empty()) { continue; }

		sol::protected_function_result res = server.luaState.safe_script(command, &sol::script_pass_on_error);

		if (!res.valid()) { std::cout << "Error while executing lua code from the server command line: " << ((sol::error)res).what() << "\n"; }
	}

	return EXIT_SUCCESS;
}
