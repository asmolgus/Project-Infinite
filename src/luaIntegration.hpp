#pragma once

#define SOL_ALL_SAFETIES_ON 1
#include <glm/glm.hpp>
#include <sol/sol.hpp>

#include <objectSystem.hpp>

namespace luaIntegration {
	enum class LuaStateType { Client, Server };

	inline sol::state createState() {
		sol::state lua;

		lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::coroutine, sol::lib::table, sol::lib::string, sol::lib::utf8);

		return lua;
	}
}
