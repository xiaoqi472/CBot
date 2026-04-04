cmake_minimum_required(VERSION 3.15)
project(cbot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 1. 收集源码
file(GLOB_RECURSE PROJECT_SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# 2. 头文件路径
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 3. 依赖管理
# cpr library
find_package(cpr QUIET)
if(NOT cpr_FOUND)
    include(FetchContent)
    FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git GIT_TAG 1.10.5)
    set(CPR_USE_SYSTEM_CURL ON) # Use system curl if available
    FetchContent_MakeAvailable(cpr)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)

# nlohmann/json library
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.2)
    FetchContent_MakeAvailable(nlohmann_json)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)