# Cracen2 - a framework for resilient streaming and distribution of independent data [![Build Status](https://travis-ci.org/ComputationalRadiationPhysics/cracen2.svg?branch=master)](https://travis-ci.org/ComputationalRadiationPhysics/cracen2)

## Dependencies

This library has the following dependencies:
- cmake/3.8.0
- gcc/6.3.0
- clang/3.8.0
- boost/1.54.0

Since it only uses C++11, gcc/5.3.0 may be enough, but it is only tested with gcc/6.3.0 and higher.

Optional

- Doxygen

## Build

The cracen2 library used cmake as build system. To build the shared library run:

```bash
mkdir build
cd build
cmake ..
make
```

For building the documentation, the option must be turned on:

```bash
cmake .. -DBUILD_DOCUMENTATION=ON
make doc
```

## Test

Cracen2 comes with a ctest testsuite. To run ctest in the build directory.

```bash
ctest
```

## Install

To install the header and shared object to the default location (usually /usr/local/include and /usr/local/lib):

```bash
make install
```

To install to a different location, a prefix path can be added. The headers will be copied to <install_prefix>/include and the shared objects to <install_prefix>/lib:
```bash
make DESTDIR=<install_prefix> install
```

## Docker

There is also a dockerfile in the project, that creates a minimal environment to build and test cracen2. It can be used with the following commands from the main directory:

```bash
docker build -t cracen2 .
docker run cracen2 # build, run tests and install cracen2
docker run -ti cracen2 /bin/bash # open an interactive session
```
