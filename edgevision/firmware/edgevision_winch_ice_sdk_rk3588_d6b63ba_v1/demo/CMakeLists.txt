cmake_minimum_required(VERSION 3.14)
project(winch_ice_customer_demo LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(EVS_SDK_DEFAULT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(EVS_SDK_ROOT "${EVS_SDK_DEFAULT}" CACHE PATH "Installed winch ice SDK directory")
find_path(EVS_INCLUDE evs/winch_ice.h PATHS "${EVS_SDK_ROOT}/include"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
find_library(EVS_ICE evs_ice PATHS "${EVS_SDK_ROOT}/lib"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
find_library(EVS_CORE evs_core PATHS "${EVS_SDK_ROOT}/lib"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
if(NOT EVS_INCLUDE OR NOT EVS_ICE OR NOT EVS_CORE)
    message(FATAL_ERROR "EVS_SDK_ROOT must contain include/evs/winch_ice.h and lib/libevs_{ice,core}.so")
endif()
add_executable(winch_ice_api_demo main.cpp recording.cpp)
target_include_directories(winch_ice_api_demo PRIVATE "${EVS_INCLUDE}")
target_link_libraries(winch_ice_api_demo PRIVATE "${EVS_ICE}" "${EVS_CORE}")
set_target_properties(winch_ice_api_demo PROPERTIES
    BUILD_RPATH "${EVS_SDK_ROOT}/lib"
    INSTALL_RPATH "$ORIGIN/../lib"
    INSTALL_RPATH_USE_LINK_PATH FALSE)
install(TARGETS winch_ice_api_demo RUNTIME DESTINATION bin)
