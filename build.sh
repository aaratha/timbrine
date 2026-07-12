
mkdir -p build
cd build

# Configure CMake (compiler flags go here)
cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ ..

# Build the project
cmake --build .
