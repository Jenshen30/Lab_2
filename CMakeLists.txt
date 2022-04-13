cmake_minimum_required(VERSION 3.10)
project(Lab_2 C)

set(CMAKE_C_STANDARD 11)

find_package(ZLIB REQUIRED)

add_executable(Lab_2 main.c)

add_library(
        isa-l
        STATIC
        ...
)

target_include_directories(isa-l PUBLIC ${CMAKE_SOURCE_DIR})


add_subdirectory(libs/isa-l)
target_link_libraries(Lab_2 PRIVATE isa-l)
#target_link_libraries(Lab_2 ${CMAKE_SOURCE_DIR}/libs/isa-l/isa-l.dll)
target_link_libraries(Lab_2 ${CMAKE_SOURCE_DIR}/cmake-build-debug/libdeflate.dll)
target_include_directories(Lab_2 PRIVATE ${CMAKE_SOURCE_DIR}/libs/isa-l)
target_link_libraries(Lab_2 ZLIB::ZLIB)
target_compile_definitions(Lab_2 PRIVATE ZLIB)