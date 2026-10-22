# Cross-compile EdgeVision for RK3576/RK3588 (aarch64) on an x86_64 Linux host.
#
#   cmake -S . -B build_rknn \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.toolchain.cmake \
#         -DEVS_BACKEND=rknn \
#         -DRKNN_API_LIB=/path/to/librknnrt.so \
#         -DRKNN_API_INCLUDE_DIR=/path/to/dir/with/rknn_api.h
#   cmake --build build_rknn -j
#
# Requires the distro cross toolchain: apt install g++-aarch64-linux-gnu
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# The board keeps librknnrt.so in /usr/lib; keep the runtime path in the binary so
# the tool can be copied next to the model and run with a bare `./eval_offline`.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)