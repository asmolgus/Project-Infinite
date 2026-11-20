#pragma once

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace assetLoader {
	inline std::string loadTextFile(const char *path) {
		std::ifstream file(path);
		if (!file.is_open()) {
			std::cerr << "Failed to open file: " << path << "\n";
			throw std::invalid_argument("Couldn't open file!");
			return "";
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}
}
