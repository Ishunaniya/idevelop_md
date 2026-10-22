"""Check camera dependency selection without requiring a target SDK or camera."""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


LIBRARIES = ["gstapp-1.0", "gstbase-1.0", "gstreamer-1.0", "gobject-2.0", "glib-2.0"]


def make_library(path):
    # A SONAME keeps CMake from rewriting the absolute SDK path as -L/-l.
    # This native stub tests configuration only, never actual target execution.
    subprocess.run(
        [
            *shlex.split(os.environ.get("CXX", "c++")),
            "-x",
            "c++",
            "-shared",
            f"-Wl,-soname,{path.name}",
            "-o",
            str(path),
            "-",
        ],
        input="",
        text=True,
        check=True,
    )


def make_sysroot(root, complete=True):
    includes = root / "usr/include"
    libraries = root / "usr/lib/aarch64-linux-gnu"
    for name in ["gstreamer-1.0/gst/gst.h", "glib-2.0/glib.h", "rga/im2d.h"]:
        path = includes / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
    config = libraries / "glib-2.0/include/glibconfig.h"
    config.parent.mkdir(parents=True)
    config.touch()
    for name in LIBRARIES + ["z", "drm", "rga"]:
        if complete or name != "gstapp-1.0":
            make_library(libraries / f"lib{name}.so")
    return libraries


def configure(cmake, source, build, root, libraries, *extra, succeeds=True):
    command = [
        cmake,
        "-S",
        str(source),
        "-B",
        str(build),
        "-G",
        "Unix Makefiles",
        "-DEVS_BACKEND=x86",
        "-DEVS_WITH_OPENCV=OFF",
        "-DEVS_WITH_GSTREAMER=ON",
        "-DEVS_WITH_RGA=ON",
        "-DEVS_BUILD_TESTS=OFF",
        "-DEVS_BUILD_TOOLS=ON",
        f"-DGSTREAMER_SYSROOT={root}",
        f"-DRGA_LIB={libraries / 'librga.so'}",
        f"-DRGA_API_INCLUDE_DIR={root / 'usr/include/rga'}",
        *extra,
    ]
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if (result.returncode == 0) != succeeds:
        raise AssertionError(result.stdout)
    return result.stdout


def check_links(build, root, drm):
    for tool in ["v4l2_detect", "evs_task_cam"]:
        directory = build / "CMakeFiles" / f"{tool}.dir"
        link = (directory / "link.txt").read_text()
        flags = (directory / "flags.make").read_text()
        for name in LIBRARIES:
            assert str(root / "usr/lib/aarch64-linux-gnu" / f"lib{name}.so") in link, link
        assert str(drm) in link, link
        assert str(root / "usr/lib/aarch64-linux-gnu/glib-2.0/include") in flags, flags
    link = (build / "CMakeFiles/v4l2_detect.dir/link.txt").read_text()
    assert str(root / "usr/lib/aarch64-linux-gnu/libz.so") in link, link


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--cmake", default="cmake")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="evs_camera_cmake_") as directory:
        temporary = Path(directory)
        root_a, root_b = temporary / "sdk_a", temporary / "sdk_b"
        libs_a, libs_b = make_sysroot(root_a), make_sysroot(root_b)
        build = temporary / "build"
        configure(args.cmake, args.source, build, root_a, libs_a)
        check_links(build, root_a, libs_a / "libdrm.so")
        configure(args.cmake, args.source, build, root_b, libs_b)
        check_links(build, root_b, libs_b / "libdrm.so")
        assert str(root_a) not in (build / "CMakeFiles/v4l2_detect.dir/link.txt").read_text()
        override = temporary / "override/libdrm.so"
        override.parent.mkdir()
        make_library(override)
        configure(args.cmake, args.source, build, root_b, libs_b, f"-DDRM_LIB={override}")
        check_links(build, root_b, override)
        configure(
            args.cmake,
            args.source,
            temporary / "rooted_build",
            root_b,
            libs_b,
            f"-DCMAKE_FIND_ROOT_PATH={temporary / 'other_root'}",
            "-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY",
            "-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY",
        )
        check_links(temporary / "rooted_build", root_b, libs_b / "libdrm.so")
        broken = temporary / "incomplete"
        libs_broken = make_sysroot(broken, complete=False)
        output = configure(
            args.cmake, args.source, temporary / "bad_build", broken, libs_broken, succeeds=False
        )
        assert "Incomplete GSTREAMER_SYSROOT" in output, output
        configure(
            args.cmake,
            args.source,
            temporary / "disabled",
            root_a,
            libs_a,
            "-DEVS_WITH_GSTREAMER=OFF",
        )
        assert not (temporary / "disabled/CMakeFiles/v4l2_detect.dir").exists()
    print(
        "Camera CMake: sysroot, reconfigure, automatic/explicit DRM, "
        "root-path isolation, incomplete SDK and OFF passed"
    )


if __name__ == "__main__":
    main()
