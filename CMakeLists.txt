cmake_minimum_required(VERSION 3.20)

project(EasyAppBase LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Boost_USE_STATIC_LIBS	ON) # only find static libs
set(Boost_USE_MULTITHREADED     ON)
set(Boost_USE_STATIC_RUNTIME    OFF)

find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)
find_package(Boost REQUIRED filesystem system url)
find_package(OpenGL REQUIRED)


add_compile_definitions(APP_NAME="${PROJECT_NAME}" SOURCE_DIR="${PROJECT_SOURCE_DIR}")

add_subdirectory(json_document)

set(JSON_ORDERED OFF CACHE BOOL "Override ordered json/xml to OFF" FORCE)

include_directories(json_document shared_recursive_mutex imgui imgui/backends ${SDL2_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR})

add_library(imgui STATIC imgui/imgui.cpp imgui/imgui.h imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_sdl2.cpp imgui/backends/imgui_impl_opengl3.cpp)

add_library(easy_app STATIC easyappbase.cpp easyappbase.hpp config.cpp app_logger.cpp app_logger.hpp HackFont.cpp utils.cpp utils.hpp app_logger.cpp app_logger.hpp network.cpp network.hpp thread.cpp thread.hpp eventhandler.cpp eventhandler.hpp)

add_executable(${PROJECT_NAME} main.cpp config.hpp)
if(TARGET SDL2::SDL2main)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE easy_app json_document imgui crypto ssl SDL2::SDL2 OpenGL::GL Boost::filesystem Boost::system Boost::url)

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)