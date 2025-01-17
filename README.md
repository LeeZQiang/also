# Advanced Logic Synthesis and  Optimization tool (ALSO)

ALSO is based on the [EPFL Logic Synthesis Libraries](https://github.com/lsils/lstools-showcase), we aim to exploit advanced logic synthesis tools for both modern FPGA and emerging nanotechnologies.

## Requirements
A modern compiler is required to build the libraries. Compiled successfully with Clang 6.0.1, GCC 7.3.0, and GCC 8.2.0. 

## How to Compile
```bash
git clone --recursive https://github.com/nbulsi/also.git
cd also
mkdir build
cd build
cmake ..
make
./build/bin/also
```

You can specify the path of the complier by extending *cmake* as follows:
```bash
cmake -DCMAKE_CXX_COMPILER=/path/to/c++-compiler ..
```

