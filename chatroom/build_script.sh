rm -rf build
rm -rf test/unit/*
mkdir -p build
cd build
cmake ..
cmake --build . -j4
cd ..