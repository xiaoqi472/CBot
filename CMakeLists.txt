# === CBOT_MANAGED_BEGIN ===
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
# cpr (lightweight library)
find_package(cpr QUIET)
if(NOT cpr_FOUND)
    include(FetchContent)
    FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git GIT_TAG 1.10.5)
    set(CPR_USE_SYSTEM_CURL ON)
    FetchContent_MakeAvailable(cpr)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE cpr::cpr)

# nlohmann/json (lightweight library)
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3)
    FetchContent_MakeAvailable(nlohmann_json)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
# === CBOT_MANAGED_END ===

# ---- 以下为用户自定义区（cbot cmake 不会修改此区域）----

# libclang（用于 cbot doc 的 C++ AST 解析）
find_path(LIBCLANG_INCLUDE_DIR clang-c/Index.h
    PATHS /usr/lib/llvm-18/include /usr/lib/llvm-17/include /usr/include
)
find_library(LIBCLANG_LIBRARY
    NAMES clang libclang
    PATHS /usr/lib/llvm-18/lib /usr/lib/llvm-17/lib /usr/lib /usr/lib/x86_64-linux-gnu
)
if(NOT LIBCLANG_INCLUDE_DIR OR NOT LIBCLANG_LIBRARY)
    message(FATAL_ERROR "未找到 libclang，请安装: sudo apt install libclang-dev")
endif()
target_include_directories(${PROJECT_NAME} PRIVATE ${LIBCLANG_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBCLANG_LIBRARY})
