#pragma once

#include <enet/enet.h>
#include <sol/sol.hpp>

#include <cmath>
#include <iostream>
#include <string>

inline void initENet() {
	if (enet_initialize() != 0) {
		std::cerr << "ENet init error!\n";
		throw std::exception();
	}
}

class GameClient {
public:
	ENetHost *clientHost = nullptr;
	ENetPeer *serverPeer = nullptr;
	bool isConnected = false;

	sol::state luaState;

	GameClient() = default;
	~GameClient() { shutdown(); }

	uint channels = 2;

	bool init() {
		if (enet_initialize() != 0) {
			std::cerr << "Failed to initialize ENet!\n";
			return false;
		}

		clientHost = enet_host_create(nullptr, 1, channels, 0, 0);
		if (!clientHost) {
			std::cerr << "Failed to create ENet client host!\n";
			return false;
		}

		return true;
	}

	bool connect(const char *ip, uint port, float timeoutSeconds = 3.0f) {
		ENetAddress address;
		enet_address_set_host(&address, ip);
		address.port = port;

		serverPeer = enet_host_connect(clientHost, &address, channels, 0);
		if (!serverPeer) {
			std::cerr << "No available peers for initiating connection!\n";
			return false;
		}

		ENetEvent event;
		if (enet_host_service(clientHost, &event, (uint)floor(timeoutSeconds * 1000.0f)) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
			std::cout << "Connected to server at " << ip << ":" << port << "\n";
			isConnected = true;
			return true;
		} else {
			std::cerr << "Connection to server timed out or refused!\n";
			enet_peer_reset(serverPeer);
			serverPeer = nullptr;
			return false;
		}
	}

	void poll() {
		if (!isConnected) return;

		ENetEvent event;
		while (enet_host_service(clientHost, &event, 0) > 0) {
			switch (event.type) {
			case ENET_EVENT_TYPE_RECEIVE: {
				std::string message(reinterpret_cast<char *>(event.packet->data), event.packet->dataLength);
				std::cout << "Server Message: " << message << "\n";

				if (message.rfind("LUA:\n", 0) == 0) {
					if (message.length() <= 5) return;
					std::string luaCode = message.substr(5);

					luaCode.erase(std::remove(luaCode.begin(), luaCode.end(), '\r'), luaCode.end());

					while (!luaCode.empty() && (luaCode.back() == '\0' || luaCode.back() == '\n')) {
						if (luaCode.back() == '\0') {
							luaCode.pop_back();
						} else {
							break;
						}
					}

					std::cout << "Executing server command!\n";

					sol::protected_function_result res = luaState.safe_script(luaCode, &sol::script_pass_on_error);

					if (!res.valid()) { std::cout << "Error while executing lua code from server: " << ((sol::error)res).what() << "\n"; }
				}

				enet_packet_destroy(event.packet);
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT: {
				std::cout << "Disconnected from server\n";
				isConnected = false;
				serverPeer = nullptr;
				break;
			}
			default: break;
			}
		}
	}

	void send(const std::string &data, uint channel = 0, bool reliable = true) {
		if (!isConnected || !serverPeer) return;

		uint flags = reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
		ENetPacket *packet = enet_packet_create(data.c_str(), data.size() + 1, flags);

		enet_peer_send(serverPeer, channel, packet);
		enet_host_flush(clientHost);
	}

	void shutdown() {
		if (serverPeer && isConnected) {
			enet_peer_disconnect(serverPeer, 0);

			ENetEvent event;
			while (enet_host_service(clientHost, &event, 1000) > 0) {
				if (event.type == ENET_EVENT_TYPE_DISCONNECT) break;
			}
		}

		if (clientHost) {
			enet_host_destroy(clientHost);
			clientHost = nullptr;
		}

		enet_deinitialize();
		isConnected = false;
	}
};
