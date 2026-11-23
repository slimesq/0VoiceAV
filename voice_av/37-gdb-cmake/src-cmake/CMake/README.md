# learning-cmake

This is a simple CMake tutorial project which contains some different scenarios.

* `hello-world`: Demo a simplest CMake project.
* `hello-world-clear`: Separate the output files and src files.
* `hello-world-lib`: Demo how to make a static/shared library.
* `hello-world-shared`: Demo how to utilize external static/shared library.
* `curl`: Demo how to use cmake with curl.
* `hello-module`: Demo how to use cmake find module.
* `config-file`: Demo how to work with config.h.
* `hunter-simple`: Demo how to use hunter and gtest.
* `boost`: Demo how to use boost library.

## Build steps
* `cmake -H. -B_builds`
* `cmake --build _builds`

## CMake based tools
* [hunter](https://github.com/ruslo/hunter): CMake-driven cross-platform package manager for C++.
* [CLion](https://www.jetbrains.com/clion/): CMake based IDE.

## Nice CMake Resources
* [CMake 实战讲义](../../../../docs/lecture_notes/37-gdb-cmake/0Voice/CMake实战-20200708.pdf)
* [CMake rules](../../../../docs/lecture_notes/37-gdb-cmake/0Voice/reference/cmake-rules.pdf)
* [Mastering CMake](../../../../docs/lecture_notes/37-gdb-cmake/0Voice/reference/mastering-cmake.pdf)
* [CGold: The Hitchhiker’s Guide to the CMake](https://cgold.readthedocs.io/en/latest/index.html)
* [Latest Official CMake doc](https://cmake.org/cmake/help/latest/index.html)
* [CMake Example](https://github.com/ttroy50/cmake-examples)
