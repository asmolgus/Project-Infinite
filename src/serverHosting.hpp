#pragma once

#include "sol/protected_function_result.hpp"
#include "sol/types.hpp"
#include <enet/enet.h>

#include <atomic>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#include <luaIntegration.hpp>

inline void initENet() {
	if (enet_initialize() != 0) {
		std::cerr << "ENet init error!\n";
		throw std::exception();
	}
}

class GameServer {
public:
	ENetHost *server = nullptr;
	std::thread serverThread;
	std::atomic<bool> isRunning{false};
	uint maxPollWaitPerTick = 15;
	uint port = 3000;
	uint channels = 2;
	uint maxClients = 32;

	sol::state luaState;

	void serverLoop() {
		ENetEvent event;
		while (isRunning) {
			while (enet_host_service(server, &event, maxPollWaitPerTick) > 0) {
				switch (event.type) {
				case ENET_EVENT_TYPE_CONNECT: std::cout << "A new client connected from " << event.peer->address.host << ":" << event.peer->address.port << "\n"; break;

				case ENET_EVENT_TYPE_RECEIVE:
					std::cout << "Packet received: " << event.packet->data << " (Length: " << event.packet->dataLength << ")\n";
					enet_packet_destroy(event.packet);
					break;

				case ENET_EVENT_TYPE_DISCONNECT:
					std::cout << "Client disconnected\n";
					event.peer->data = NULL;
					break;

				default: break;
				}
			}
		}
	}

	GameServer() = default;
	~GameServer() { stop(); }

	void executeLuaFile(std::string path) {
		sol::protected_function_result res = luaState.do_file(path, sol::load_mode::text);
		if (!res.valid()) { std::cout << "File execution error: " << ((sol::error)res).what() << "\n"; }
	}

	bool start() {
		if (isRunning) return false;

		ENetAddress address;
		address.host = ENET_HOST_ANY;
		address.port = port;

		server = enet_host_create(&address, maxClients, channels, 0, 0);
		if (!server) {
			std::cerr << "Failed to create ENet server host!\n";
			return false;
		}

		std::cout << "Server started on port " << std::to_string(port) << "\n";
		isRunning = true;

		serverThread = std::thread(&GameServer::serverLoop, this);
		return true;
	}

	void stop() {
		if (!isRunning) return;
		isRunning = false;

		if (serverThread.joinable()) serverThread.join();

		if (server) {
			enet_host_destroy(server);
			server = nullptr;
		}
		std::cout << "Server stopped\n";
	}

	void sendMessage(ENetPeer *peer, const std::string &message, uint channel = 0, bool reliable = true) {
		if (!peer || !isRunning) return;
		uint32_t flags = reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
		ENetPacket *packet = enet_packet_create(message.c_str(), message.size() + 1, flags);
		enet_peer_send(peer, channel, packet);
		enet_host_flush(server);
	}

	void broadcastMessage(const std::string &message, uint channel = 0, bool reliable = true) {
		if (!server || !isRunning) return;
		uint flags = reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
		ENetPacket *packet = enet_packet_create(message.c_str(), message.size() + 1, flags);
		enet_host_broadcast(server, channel, packet);
		enet_host_flush(server);
	}

	void sendLua(ENetPeer *peer, const std::string &luaCode, uint channel = 0) {
		std::string payload = "LUA:\n" + luaCode;
		sendMessage(peer, payload, channel, true);
	}

	void broadcastLua(const std::string &luaCode, uint channel = 0) {
		std::string payload = "LUA:\n" + luaCode;
		broadcastMessage(payload, channel, true);
	}
};
