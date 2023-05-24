cmake_minimum_required(VERSION 3.20)

set(CMAKE_TOOLCHAIN_FILE "C:/code/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(HYPERVADMINISSUE VERSION 1.0)

add_compile_definitions(UNICODE _UNICODE)
add_compile_options(/std:c++latest)

add_executable(${PROJECT_NAME})

find_package(Boost REQUIRED COMPONENTS Json)

target_sources(${PROJECT_NAME}
    PRIVATE
        main.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${Boost_INCLUDE_DIRS}
)

target_link_directories(${PROJECT_NAME}
    PUBLIC
        ${Boost_LIBRARY_DIRS}
)
    
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Rpcrt4.lib
        ${Boost_LIBRARIES}
)