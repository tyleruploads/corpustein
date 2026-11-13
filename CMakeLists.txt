cmake_minimum_required(VERSION 3.16)
project(corpustein VERSION 0.5.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(corpustein
    src/main.cpp
    src/function_runner.inc
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
    -Wshadow
    -Werror
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(corpustein PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(corpustein PRIVATE
        -fsanitize=address,undefined
    )

    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set_target_properties(corpustein PROPERTIES
            CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-p=${CMAKE_BINARY_DIR}"
        )
    endif()

    find_program(CLANG_FORMAT_EXE NAMES clang-format)
    find_program(CPPCHECK_EXE NAMES cppcheck)

    if(CLANG_FORMAT_EXE AND CPPCHECK_EXE)
        add_custom_target(enforce_quality_checks
            COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
            COMMAND ${CPPCHECK_EXE}
                --project=${CMAKE_BINARY_DIR}/compile_commands.json
                --enable=all
                --suppress=missingIncludeSystem
                --check-level=exhaustive
                --error-exitcode=1
            COMMENT "Running Clang-Format and Cppcheck"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            USES_TERMINAL
        )

        add_dependencies(corpustein enforce_quality_checks)
    endif()
endif()

target_compile_definitions(corpustein PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    PROJECT_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

install(TARGETS corpustein RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/corpora/"
        DESTINATION /usr/local/share/corpustein/corpora
    )
else()
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/corpora"
        DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
