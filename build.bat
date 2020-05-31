rem Use this batch file to build YoshiPBR for Visual Studio
rmdir /s /q build
mkdir build
cd build
cmake .. -DCMAKE_GENERATOR_PLATFORM=x64
cmake --build .
start YoshiPBR.sln
