cmake_minimum_required(VERSION 3.4)

project(GatewayChanger)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${PROJECT_SOURCE_DIR}/include)
add_subdirectory(lib/nlohmann_json)

add_executable(GatewayChanger src/main.cpp)
target_link_libraries(GatewayChanger PRIVATE nlohmann_json::nlohmann_json user32.lib ws2_32.lib iphlpapi.lib)

add_executable(GatewayChanger_test src/test.cpp)
target_link_libraries(GatewayChanger_test user32.lib ws2_32.lib iphlpapi.lib)
