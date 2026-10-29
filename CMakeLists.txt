cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(deps)

set(TARGET CaretakersForge)
project(${TARGET})

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(${TARGET} SHARED ${SRC_FILES})
target_include_directories(${TARGET} PRIVATE
    include/
)
target_link_libraries(${TARGET} PRIVATE nlohmann_json::nlohmann_json UE4SS)
