cmake_minimum_required(VERSION 3.20)

project(open_tui_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OPEN_TUI_BUILD_EXAMPLE "Build example debugger executable" ON)

add_library(open_tui_cpp
  src/command_registry.cpp
  src/console.cpp
  src/line_editor.cpp
  src/signal_manager.cpp
  src/tui_application.cpp
  src/udp_client.cpp
)

add_library(open_tui_cpp::open_tui_cpp ALIAS open_tui_cpp)

target_include_directories(open_tui_cpp
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(open_tui_cpp PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus)
else()
  target_compile_options(open_tui_cpp PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

if(WIN32)
  target_link_libraries(open_tui_cpp PUBLIC ws2_32)
endif()

if(OPEN_TUI_BUILD_EXAMPLE)
  add_executable(open_tui_example examples/debugger/main.cpp)
  target_link_libraries(open_tui_example PRIVATE open_tui_cpp::open_tui_cpp)
endif()
