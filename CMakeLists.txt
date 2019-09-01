cmake_minimum_required(VERSION 3.4)

project(GatewayChanger)

include_directories("${PROJECT_SOURCE_DIR}/include")

add_executable(GatewayChanger src/main.cpp)
target_link_libraries(GatewayChanger user32.lib ws2_32.lib iphlpapi.lib)

add_executable(GatewayChanger_test src/test.cpp)
target_link_libraries(GatewayChanger_test user32.lib ws2_32.lib iphlpapi.lib)
