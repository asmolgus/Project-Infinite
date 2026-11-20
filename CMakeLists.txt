cmake_minimum_required(VERSION 3.20)
project(Project-Infinite LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LUA REQUIRED lua54)
pkg_check_modules(ENET REQUIRED libenet)

find_package(SDL3 REQUIRED CONFIG)

add_library(glad STATIC include/glad/gl.c)
target_include_directories(glad PUBLIC include)

add_executable(Client
	src/client.cpp
)
add_executable(Server
	src/server.cpp
)

target_include_directories(Client PRIVATE 
	include 
	src 
	${LUA_INCLUDE_DIRS}
	${ENET_INCLUDE_DIRS}
)
target_include_directories(Server PRIVATE 
	include 
	src 
	${LUA_INCLUDE_DIRS}
	${ENET_INCLUDE_DIRS}
)

target_link_libraries(Client PRIVATE
	SDL3::SDL3
	glad
	${LUA_LIBRARIES}
	${ENET_LIBRARIES}
)
target_link_libraries(Server PRIVATE
	${LUA_LIBRARIES}
	${ENET_LIBRARIES}
)

if(UNIX AND NOT APPLE)
	target_link_libraries(Client PRIVATE ${CMAKE_DL_LIBS} ${ENET_LDFLAGS})
	target_link_libraries(Server PRIVATE ${CMAKE_DL_LIBS} ${ENET_LDFLAGS})
endif()

if(WIN32)
	add_custom_command(TARGET Client POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		$<TARGET_FILE:SDL3::SDL3>
		$<TARGET_FILE_DIR:Client>
	)
endif()

add_custom_target(sync_assets ALL
	COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:Client>/assets"
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_SOURCE_DIR}/assets"
		"$<TARGET_FILE_DIR:Client>/assets"
)
add_dependencies(Client sync_assets)
