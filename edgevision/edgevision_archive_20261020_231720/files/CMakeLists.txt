cmake_minimum_required(VERSION 3.14)
project(edgevision CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Host-side builds (MSVC) need UTF-8 sources: the comments are Chinese, and
# without /utf-8 the compiler mis-parses them on a codepage-936 Windows box.
if(MSVC)
    add_compile_options(/utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Backend selection: rknn | qnn | x86 | onnx | mock
#   mock : dependency-free placeholder (also always compiled, it is the CI/test backend)
#   x86  : host CPU backend -- reference detector by default, ONNX when ORT linked
#   onnx : same x86 backend, but built to load the real exported ONNX model
#   rknn : Rockchip NPU backend (needs the RKNN SDK)
set(EVS_BACKEND "mock" CACHE STRING "Inference backend: rknn | qnn | x86 | onnx | mock")
message(STATUS "EVS_BACKEND = ${EVS_BACKEND}")

# mock is the universal, dependency-free backend AND the CI/test backend. It is
# always compiled (see EVS_SRC below), so its factory branch must always exist.
add_definitions(-DEVS_BACKEND_MOCK)

# ONNXRuntime for the x86/onnx production path. Off by default so the repo builds
# with zero third-party dependencies; flip on and point -DONNXRUNTIME_LIB_DIR at
# the ORT lib folder to get real-model offline accuracy.
option(EVS_WITH_ORT "Build the x86 backend with ONNXRuntime (real model)" OFF)
option(EVS_WITH_OPENCV "Enable OpenCV in height/ice plugins" OFF)
if(EVS_WITH_OPENCV)
    add_compile_definitions(EVS_WITH_OPENCV)
endif()

set(EVS_SRC
    src/core/config.cpp
    src/core/monotonic_clock.cpp
    src/core/model_registry.cpp
    src/core/backend_factory.cpp
    src/core/detection_decoder.cpp
    src/core/preprocess.cpp
    src/core/log.cpp
    src/core/debug.cpp
    src/core/offline_eval.cpp
    src/vision_task.cpp
    src/tasks/function_plugin_factory.cpp
    src/ground/homography_mapper.cpp
    src/ground/crane_pose_mapper.cpp
    src/ground/ground_mapper_factory.cpp
    src/ground/ground_params.cpp
    src/ground/ground_metrics.cpp
    # mock is the universal, dependency-free backend and the test backend: always present.
    src/backend/mock/mock_backend.cpp
)

if(EVS_BACKEND STREQUAL "rknn")
    list(APPEND EVS_SRC src/backend/rknn/rknn_backend.cpp)
    add_definitions(-DEVS_BACKEND_RKNN)
elseif(EVS_BACKEND STREQUAL "x86")
    list(APPEND EVS_SRC src/backend/x86/x86_backend.cpp)
    add_definitions(-DEVS_BACKEND_X86)
elseif(EVS_BACKEND STREQUAL "onnx")
    list(APPEND EVS_SRC src/backend/x86/x86_backend.cpp)
    add_definitions(-DEVS_BACKEND_ONNX)
elseif(EVS_BACKEND STREQUAL "mock")
    # mock is already in EVS_SRC; nothing else to add.
else()
    message(WARNING "EVS_BACKEND=${EVS_BACKEND} is not implemented in the scaffold. "
            "Add the SDK backend sources to EVS_SRC and a matching create_backend() branch "
            "in src/core/backend_factory.cpp before building for the target platform.")
endif()

# The x86 / onnx backends share the same translation unit; when ORT is requested
# and found, compile the real-model path and link onnxruntime. Detection only
# here -- the actual target_link_libraries / include dirs are applied AFTER
# add_library(evs) below (a target cannot be linked before it exists).
# NOTE: never pre-`set()` the result variables here. find_library/find_path skip
# their search when the variable is already DEFINED -- even as an empty normal
# variable -- which silently disables the ORT branch.
if(EVS_WITH_ORT AND (EVS_BACKEND STREQUAL "x86" OR EVS_BACKEND STREQUAL "onnx"))
    find_library(EVS_ORT_LIB NAMES onnxruntime
                 HINTS ${ONNXRUNTIME_LIB_DIR} /usr/lib /usr/local/lib)
    find_path(EVS_ORT_INCLUDE NAMES onnxruntime_cxx_api.h
              HINTS ${ONNXRUNTIME_LIB_DIR}/../include
                    ${ONNXRUNTIME_INCLUDE_DIR}
                    /usr/include /usr/local/include)
    if(EVS_ORT_LIB)
        add_definitions(-DEVS_WITH_ORT)
        message(STATUS "ONNXRuntime enabled: ${EVS_ORT_LIB}")
        if(EVS_ORT_INCLUDE)
            message(STATUS "ONNXRuntime include: ${EVS_ORT_INCLUDE}")
        else()
            message(WARNING "onnxruntime_cxx_api.h not found; pass "
                    "-DONNXRUNTIME_INCLUDE_DIR=/path/to/ort/include.")
        endif()
    else()
        message(WARNING "EVS_WITH_ORT=ON but libonnxruntime not found. Pass "
                "-DONNXRUNTIME_LIB_DIR=/path/to/ort/lib, or build the reference backend.")
    endif()
endif()

# Compile the sources once, with position-independent code, then pack them into
# both libevs.a and libevs_core.so. Tools and tests keep linking the static target.
add_library(evs_objs OBJECT ${EVS_SRC})
set_target_properties(evs_objs PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(evs_objs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(evs_objs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

option(EVS_BUILD_SHARED "Also build libevs_core.so for host applications" ON)

add_library(evs STATIC $<TARGET_OBJECTS:evs_objs>)
target_include_directories(evs PUBLIC
    $<TARGET_PROPERTY:evs_objs,INTERFACE_INCLUDE_DIRECTORIES>)

set(EVS_LIB_TARGETS evs)
if(EVS_BUILD_SHARED)
    add_library(evs_shared SHARED $<TARGET_OBJECTS:evs_objs>)
    set_target_properties(evs_shared PROPERTIES
        OUTPUT_NAME evs_core
        SOVERSION 3
        # Do not bake the build machine's SDK directory into the library.
        # On the board, librknnrt.so is already on the default linker path.
        SKIP_BUILD_RPATH TRUE
        BUILD_WITH_INSTALL_RPATH FALSE
        INSTALL_RPATH ""
        INSTALL_RPATH_USE_LINK_PATH FALSE)
    target_include_directories(evs_shared PUBLIC
        $<TARGET_PROPERTY:evs_objs,INTERFACE_INCLUDE_DIRECTORIES>)
    if(UNIX AND NOT APPLE)
        # Fail the shared link if a symbol was left for the host to supply.
        target_link_options(evs_shared PRIVATE "LINKER:--no-undefined")
    endif()
    list(APPEND EVS_LIB_TARGETS evs_shared)
endif()

# One shared library per function. Each .so registers its module_id when it is
# loaded, so a host links only the functions it ships. The static archives are
# for tests and tools; they must be whole-archived or the registrar is dropped.
function(evs_add_plugin name)
    add_library(${name}_objs OBJECT ${ARGN})
    set_target_properties(${name}_objs PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(${name}_objs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_include_directories(${name}_objs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_library(${name} STATIC $<TARGET_OBJECTS:${name}_objs>)
    if(EVS_BUILD_SHARED)
        add_library(${name}_shared SHARED $<TARGET_OBJECTS:${name}_objs>)
        set_target_properties(${name}_shared PROPERTIES
            OUTPUT_NAME ${name}
            SOVERSION 3
            # Look for libevs_core.so.3 beside this plugin, not on the build machine.
            BUILD_RPATH "\$ORIGIN"
            INSTALL_RPATH "\$ORIGIN"
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH_USE_LINK_PATH FALSE)
        target_link_libraries(${name}_shared PRIVATE evs_shared)
        if(UNIX AND NOT APPLE)
            target_link_options(${name}_shared PRIVATE "LINKER:--no-undefined")
        endif()
    endif()
endfunction()

evs_add_plugin(evs_person src/tasks/person/person_plugin.cpp)
evs_add_plugin(evs_height_limit
    src/tasks/height_limit/height_limit_plugin.cpp
    src/tasks/height_limit/height_limit_mount.cpp)
evs_add_plugin(evs_ice
    src/tasks/winch_ice/winch_ice_plugin.cpp
    src/tasks/winch_ice/ice_measurement.cpp
    src/tasks/winch_ice/winch_ice_api.cpp)

if(EVS_WITH_OPENCV)
    find_package(OpenCV QUIET COMPONENTS core imgproc)
    if(OpenCV_FOUND)
        set(EVS_CV_INCLUDE ${OpenCV_INCLUDE_DIRS})
        set(EVS_CV_LIBS ${OpenCV_LIBS})
    else()
        find_path(EVS_CV_INCLUDE opencv2/core.hpp PATH_SUFFIXES opencv4)
        find_library(EVS_CV_CORE NAMES opencv_core)
        find_library(EVS_CV_IMGPROC NAMES opencv_imgproc)
        if(NOT EVS_CV_INCLUDE OR NOT EVS_CV_CORE OR NOT EVS_CV_IMGPROC)
            message(FATAL_ERROR "EVS_WITH_OPENCV needs target OpenCV core/imgproc headers and libraries")
        endif()
        set(EVS_CV_LIBS ${EVS_CV_IMGPROC} ${EVS_CV_CORE})
    endif()
    foreach(_name evs_ice evs_height_limit)
        target_include_directories(${_name}_objs PRIVATE ${EVS_CV_INCLUDE})
        target_include_directories(${_name} PUBLIC ${EVS_CV_INCLUDE})
        target_link_libraries(${_name} PUBLIC ${EVS_CV_LIBS})
        if(EVS_BUILD_SHARED)
            target_include_directories(${_name}_shared PUBLIC ${EVS_CV_INCLUDE})
            target_link_libraries(${_name}_shared PRIVATE ${EVS_CV_LIBS})
        endif()
    endforeach()
endif()

# Decoder and ground-mapper objects still register from static initializers
# inside libevs. Plugin objects do the same from their own archives. A normal
# link of a .a drops any .o that nothing calls, so static consumers take the
# whole archive. The shared libraries are built from their object lists and
# already contain those initializers.
function(evs_link_whole target)
    if(MSVC)
        target_link_libraries(${target} PRIVATE evs evs_person evs_height_limit evs_ice)
        target_link_options(${target} PRIVATE
            "/WHOLEARCHIVE:$<TARGET_FILE:evs>"
            "/WHOLEARCHIVE:$<TARGET_FILE:evs_person>"
            "/WHOLEARCHIVE:$<TARGET_FILE:evs_height_limit>"
            "/WHOLEARCHIVE:$<TARGET_FILE:evs_ice>")
    else()
        target_link_libraries(${target} PRIVATE
            "-Wl,--whole-archive"
            evs evs_person evs_height_limit evs_ice
            "-Wl,--no-whole-archive")
    endif()
endfunction()

function(evs_link_public)
    foreach(_t IN LISTS EVS_LIB_TARGETS)
        target_link_libraries(${_t} PUBLIC ${ARGN})
    endforeach()
endfunction()

# Backend-specific SDK runtime libraries (after add_library, never before).
if(EVS_ORT_LIB)
    evs_link_public(${EVS_ORT_LIB})
endif()
if(EVS_ORT_INCLUDE)
    target_include_directories(evs_objs PUBLIC ${EVS_ORT_INCLUDE})
endif()

# Backend-specific SDK runtime libraries (after add_library, never before).
if(EVS_BACKEND STREQUAL "rknn")
    # Override with -DRKNN_API_LIB=/path/to/librknnrt.so when the runtime is
    # elsewhere. On the board the runtime lives at /usr/lib/librknnrt.so; a cross
    # build host must point at a copy pulled off the board, because the linker
    # cannot use a library that only exists on the target.
    if(DEFINED RKNN_API_LIB)
        set(EVS_RKNN_LIB "${RKNN_API_LIB}")
    else()
        find_library(EVS_RKNN_LIB NAMES rknnrt rknn_api
                     HINTS ${RKNN_API_LIB_DIR}
                           /usr/lib /usr/local/lib /usr/lib/aarch64-linux-gnu)
    endif()
    if(EVS_RKNN_LIB)
        evs_link_public(${EVS_RKNN_LIB})
    else()
        message(WARNING "librknnrt.so not found. Install the RKNN runtime 2.x on the "
                "board, or pass -DRKNN_API_LIB=/path/to/librknnrt.so when cross building.")
    endif()
    # rknn_api.h: pass -DRKNN_API_INCLUDE_DIR=<dir containing rknn_api.h>.
    if(DEFINED RKNN_API_INCLUDE_DIR)
        target_include_directories(evs_objs PUBLIC ${RKNN_API_INCLUDE_DIR})
    else()
        find_path(EVS_RKNN_INCLUDE NAMES rknn_api.h
                  HINTS /usr/include /usr/local/include /usr/include/rknn_api)
        if(EVS_RKNN_INCLUDE)
            target_include_directories(evs_objs PUBLIC ${EVS_RKNN_INCLUDE})
        else()
            message(WARNING "rknn_api.h not found. Pass -DRKNN_API_INCLUDE_DIR=<dir>.")
        endif()
    endif()
endif()

# Rockchip RGA (hardware scale/colour-convert) for the host-side tools.
# Deliberately NOT part of the `evs` library: the library stays free of Rockchip
# headers so it builds on a bare PC and cross-compiles anywhere. Only
# tools/v4l2_detect links it, which is where a product host would put its
# capture-side acceleration.
option(EVS_WITH_RGA "Build the host capture tool with Rockchip RGA preprocessing" OFF)
# GStreamer for the gst/shm capture paths in v4l2_detect (--shm / --config+--channel
# gstreamer capture). Needs the board sysroot dev packages (see wiring below).
option(EVS_WITH_GSTREAMER "Link GStreamer into v4l2_detect (gst/shm capture)" ON)
set(EVS_RGA_LIB "")
set(EVS_RGA_INCLUDE_DIRS "")
if(EVS_WITH_RGA AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(DEFINED RGA_LIB)
        set(EVS_RGA_LIB "${RGA_LIB}")
    else()
        find_library(EVS_RGA_LIB NAMES rga HINTS /usr/lib /usr/local/lib)
    endif()
    # Needs BOTH dirs: include/ (rga.h, RockchipRga.h) and im2d_api/ (im2d.h).
    if(DEFINED RGA_API_INCLUDE_DIR)
        set(EVS_RGA_INCLUDE_DIRS "${RGA_API_INCLUDE_DIR}")
    else()
        find_path(EVS_RGA_IM2D HINTS /usr/include /usr/local/include
                  PATH_SUFFIXES im2d_api linux-rga/im2d_api)
        find_path(EVS_RGA_BASE HINTS /usr/include /usr/local/include
                  PATH_SUFFIXES linux-rga rga)
        if(EVS_RGA_IM2D AND EVS_RGA_BASE)
            get_filename_component(EVS_RGA_IM2D_PARENT "${EVS_RGA_IM2D}" DIRECTORY)
            set(EVS_RGA_INCLUDE_DIRS "${EVS_RGA_IM2D_PARENT}" "${EVS_RGA_BASE}")
        endif()
    endif()
endif()

# Independent controls; camera tools do not require a model backend.
option(EVS_BUILD_TESTS "Build regression tests" ON)
option(EVS_BUILD_TOOLS "Build demos and evaluation tools" ON)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    if(EVS_BUILD_TESTS)
        enable_testing()
        if(UNIX AND NOT APPLE)
            find_package(Python3 COMPONENTS Interpreter QUIET)
            if(Python3_Interpreter_FOUND)
                add_test(NAME evs_camera_cmake COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_camera_cmake.py
                    ${CMAKE_CURRENT_SOURCE_DIR} --cmake ${CMAKE_COMMAND})
            endif()
        endif()
        add_executable(evs_tests tests/test_core.cpp)
        evs_link_whole(evs_tests)
        add_test(NAME evs_tests COMMAND evs_tests)
        add_executable(evs_ice_tests tests/test_ice.cpp)
        target_include_directories(evs_ice_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        evs_link_whole(evs_ice_tests)
        target_compile_definitions(evs_ice_tests PRIVATE EVS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME evs_ice_tests COMMAND evs_ice_tests)
        if(EVS_WITH_OPENCV)
            add_executable(evs_ice_allocation_tests tests/test_ice_allocation.cpp)
            target_include_directories(evs_ice_allocation_tests PRIVATE src ${EVS_CV_INCLUDE})
            if(EVS_BUILD_SHARED)
                # Logger/update faults use a generic person task, not an ice bypass.
                target_sources(evs_ice_allocation_tests PRIVATE $<TARGET_OBJECTS:evs_person_objs>)
                target_include_directories(evs_ice_allocation_tests PRIVATE include)
                target_link_libraries(evs_ice_allocation_tests PRIVATE evs_ice_shared evs_shared ${EVS_CV_LIBS})
                set_target_properties(evs_ice_allocation_tests PROPERTIES BUILD_RPATH "\$ORIGIN")
            else()
                evs_link_whole(evs_ice_allocation_tests)
            endif()
            add_test(NAME evs_ice_allocation_tests COMMAND evs_ice_allocation_tests)
        endif()
        if(EVS_BUILD_SHARED)
            add_executable(evs_ice_dynamic_tests tests/test_ice.cpp)
            target_include_directories(evs_ice_dynamic_tests PRIVATE include src ${EVS_CV_INCLUDE})
            target_compile_definitions(evs_ice_dynamic_tests PRIVATE EVS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
            target_link_libraries(evs_ice_dynamic_tests PRIVATE evs_ice_shared evs_shared ${EVS_CV_LIBS})
            set_target_properties(evs_ice_dynamic_tests PROPERTIES BUILD_RPATH "\$ORIGIN")
            add_test(NAME evs_ice_dynamic_tests COMMAND evs_ice_dynamic_tests)
        endif()
    endif()
    if(EVS_BUILD_TOOLS AND EVS_WITH_OPENCV)
        add_executable(ice_replay tools/ice_replay/main.cpp)
        target_include_directories(ice_replay PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        if(EVS_BUILD_SHARED)
            # Direct Detector symbols ensure libevs_ice cannot be pruned by --as-needed.
            target_link_libraries(ice_replay PRIVATE evs_ice_shared evs_shared ${EVS_CV_LIBS})
            target_include_directories(ice_replay PRIVATE ${EVS_CV_INCLUDE})
            set_target_properties(ice_replay PROPERTIES BUILD_RPATH "\$ORIGIN;\$ORIGIN/../lib")
        else()
            evs_link_whole(ice_replay)
        endif()
        if(EVS_BUILD_TESTS)
            find_package(Python3 COMPONENTS Interpreter QUIET)
            if(Python3_Interpreter_FOUND)
                add_test(NAME evs_ice_cli COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_ice_cli.py
                    $<TARGET_FILE:ice_replay> ${CMAKE_CURRENT_SOURCE_DIR})
            endif()
        endif()
    endif()
    if(EVS_BUILD_TOOLS AND (EVS_BACKEND STREQUAL "x86" OR EVS_BACKEND STREQUAL "onnx" OR EVS_BACKEND STREQUAL "rknn"))
        add_executable(eval_offline tools/eval_offline/main.cpp)
        evs_link_whole(eval_offline)
    endif()
    if(EVS_BUILD_TOOLS AND EVS_WITH_GSTREAMER AND UNIX AND NOT APPLE)
        # An explicit target sysroot takes precedence over the host pkg-config.
        # Keep the legacy argument usable without adding a target libc to -L.
        if(GSTREAMER_SYSROOT)
            set(_gst_inc "${GSTREAMER_SYSROOT}/usr/include")
            set(_gst_lib_paths
                "${GSTREAMER_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
                "${GSTREAMER_SYSROOT}/usr/lib/aarch64-linux-gnu"
                "${GSTREAMER_SYSROOT}/usr/lib")
            unset(EVS_GST_CONFIG_INCLUDE CACHE)
            find_path(EVS_GST_CONFIG_INCLUDE glibconfig.h
                PATHS ${_gst_lib_paths} PATH_SUFFIXES glib-2.0/include
                NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
            set(EVS_GST_FOUND TRUE)
            set(_gst_libraries "")
            foreach(_lib gstapp-1.0 gstbase-1.0 gstreamer-1.0 gobject-2.0 glib-2.0)
                # Re-resolve on reconfigure if GSTREAMER_SYSROOT has changed.
                unset(_gst_library CACHE)
                find_library(_gst_library NAMES ${_lib}
                    PATHS ${_gst_lib_paths} NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
                if(NOT _gst_library)
                    set(EVS_GST_FOUND FALSE)
                endif()
                list(APPEND _gst_libraries "${_gst_library}")
            endforeach()
            if(NOT EXISTS "${_gst_inc}/gstreamer-1.0/gst/gst.h" OR
               NOT EXISTS "${_gst_inc}/glib-2.0/glib.h" OR NOT EVS_GST_CONFIG_INCLUDE)
                set(EVS_GST_FOUND FALSE)
            endif()
            if(NOT EVS_GST_FOUND)
                message(FATAL_ERROR "Incomplete GSTREAMER_SYSROOT=${GSTREAMER_SYSROOT}: need target GStreamer/glib headers and libraries")
            endif()
            add_library(evs_gstreamer INTERFACE)
            target_include_directories(evs_gstreamer SYSTEM INTERFACE
                "${_gst_inc}/gstreamer-1.0" "${_gst_inc}/glib-2.0"
                "${EVS_GST_CONFIG_INCLUDE}")
            target_link_libraries(evs_gstreamer INTERFACE ${_gst_libraries})
            message(STATUS "GStreamer target sysroot: ${GSTREAMER_SYSROOT}")
        else()
            find_package(PkgConfig QUIET)
            if(PkgConfig_FOUND)
                pkg_check_modules(EVS_GST QUIET IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0)
            endif()
            if(EVS_GST_FOUND)
                add_library(evs_gstreamer INTERFACE)
                target_link_libraries(evs_gstreamer INTERFACE PkgConfig::EVS_GST)
            endif()
        endif()
        if(EVS_GST_FOUND)
            find_package(Threads REQUIRED)
            if(EVS_WITH_OPENCV)
                add_executable(winch_ice_demo tools/winch_ice_demo/main.cpp tools/v4l2_detect/capture.cpp)
                target_include_directories(winch_ice_demo PRIVATE tools/v4l2_detect tools/ice_replay)
                if(EVS_BUILD_SHARED)
                    target_link_libraries(winch_ice_demo PRIVATE evs_ice_shared evs_shared ${EVS_CV_LIBS})
                    target_include_directories(winch_ice_demo PRIVATE ${EVS_CV_INCLUDE})
                    set_target_properties(winch_ice_demo PROPERTIES BUILD_RPATH "\$ORIGIN;\$ORIGIN/../lib")
                else()
                    evs_link_whole(winch_ice_demo)
                endif()
                target_link_libraries(winch_ice_demo PRIVATE evs_gstreamer Threads::Threads)
            endif()
            if(NOT EVS_BACKEND STREQUAL "mock")
                # Legacy cross sysroots also carry zlib for the PNG writer.
                if(GSTREAMER_SYSROOT)
                    unset(EVS_CAMERA_ZLIB CACHE)
                    find_library(EVS_CAMERA_ZLIB NAMES z PATHS ${_gst_lib_paths}
                        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
                    if(NOT EVS_CAMERA_ZLIB)
                        message(FATAL_ERROR "GSTREAMER_SYSROOT needs target libz for v4l2_detect")
                    endif()
                else()
                    find_package(ZLIB REQUIRED)
                    set(EVS_CAMERA_ZLIB ZLIB::ZLIB)
                endif()
                add_executable(v4l2_detect tools/v4l2_detect/main.cpp tools/v4l2_detect/rga_preprocess.cpp
                    tools/v4l2_detect/capture.cpp tools/v4l2_detect/detection_shm.cpp)
                evs_link_whole(v4l2_detect)
                target_link_libraries(v4l2_detect PRIVATE evs_gstreamer Threads::Threads ${EVS_CAMERA_ZLIB})
                add_executable(evs_task_cam tools/task_cam/main.cpp tools/v4l2_detect/capture.cpp
                    tools/v4l2_detect/rga_preprocess.cpp)
                target_include_directories(evs_task_cam PRIVATE tools/v4l2_detect)
                evs_link_whole(evs_task_cam)
                target_link_libraries(evs_task_cam PRIVATE evs_gstreamer Threads::Threads)
                if(EVS_WITH_RGA)
                    if(DEFINED DRM_LIB)
                        set(EVS_DRM_LIB "${DRM_LIB}")
                    else()
                        unset(EVS_DRM_LIB CACHE)
                        if(GSTREAMER_SYSROOT)
                            find_library(EVS_DRM_LIB NAMES drm HINTS ${RGA_LIB_DIR}
                                PATHS ${_gst_lib_paths} NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
                        else()
                            find_library(EVS_DRM_LIB NAMES drm
                                HINTS ${RGA_LIB_DIR} /usr/lib /usr/lib/aarch64-linux-gnu)
                        endif()
                    endif()
                    if(NOT EVS_RGA_INCLUDE_DIRS OR NOT EVS_RGA_LIB)
                        message(WARNING "RGA requested: pass RGA_API_INCLUDE_DIR and RGA_LIB if target headers/libraries are missing")
                    endif()
                    if(NOT EVS_DRM_LIB)
                        message(WARNING "librga needs libdrm; pass -DDRM_LIB=/path/to/target/libdrm.so")
                    endif()
                    foreach(_tool v4l2_detect evs_task_cam)
                        target_compile_definitions(${_tool} PRIVATE EVS_WITH_RGA)
                        if(EVS_RGA_INCLUDE_DIRS)
                            target_include_directories(${_tool} PRIVATE ${EVS_RGA_INCLUDE_DIRS})
                        endif()
                        if(EVS_RGA_LIB)
                            target_link_libraries(${_tool} PRIVATE ${EVS_RGA_LIB})
                        endif()
                        if(EVS_DRM_LIB)
                            target_link_libraries(${_tool} PRIVATE ${EVS_DRM_LIB})
                        endif()
                    endforeach()
                endif()
            endif()
        else()
            message(WARNING "GStreamer development files missing: camera tools omitted; library/replay remain available")
        endif()
    endif()
endif()

include(GNUInstallDirs)
# Follow GNUInstallDirs for relocatable lib, lib64 and multiarch layouts.
file(RELATIVE_PATH EVS_INSTALL_LIB_FROM_BIN
    "${CMAKE_INSTALL_FULL_BINDIR}"
    "${CMAKE_INSTALL_FULL_LIBDIR}")
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS evs evs_ice ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(EVS_BUILD_SHARED)
    install(TARGETS evs_shared evs_ice_shared LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
foreach(_tool ice_replay winch_ice_demo)
    if(TARGET ${_tool})
        install(TARGETS ${_tool} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        set_target_properties(${_tool} PROPERTIES INSTALL_RPATH "\$ORIGIN/${EVS_INSTALL_LIB_FROM_BIN}")
    endif()
endforeach()
install(FILES configs/winch_ice.yaml
    DESTINATION share/edgevision/configs)
install(FILES docs/winch_ice.md
    DESTINATION share/edgevision/docs)
