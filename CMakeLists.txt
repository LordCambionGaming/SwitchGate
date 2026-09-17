cmake_minimum_required(VERSION 3.16)
set(CMAKE_POLICY_VERSION_MINIMUM 3.16)
project(Puzzle3D CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Scarica raylib automaticamente (nessuna installazione manuale richiesta) ---
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(raylib)

# --- Scarica nlohmann/json (header-only) per leggere i livelli in formato JSON ---
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/Level.cpp
    src/Config.cpp
    src/Scores.cpp
    src/Editor.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib nlohmann_json::nlohmann_json)

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
    # Evita di aprire una console nera insieme alla finestra di gioco (opzionale)
    # set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if (APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# --- Copia le cartelle "levels" e "assets" accanto all'eseguibile a ogni build,
#     cosi' i livelli JSON e la musica vengono trovati a runtime. ---
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/levels" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/levels"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copio levels/ e assets/ nella cartella di output"
)
