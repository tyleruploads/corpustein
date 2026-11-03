cmake_minimum_required(VERSION 3.16)
project(corpustein VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(corpustein
    src/main.cpp
)

add_custom_command(
    TARGET corpustein POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/src/corpora"
        "${CMAKE_BINARY_DIR}/corpora"
    COMMENT "Copying corpora to build directory"
)

target_compile_options(corpustein PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

target_compile_definitions(corpustein PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    PROJECT_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
