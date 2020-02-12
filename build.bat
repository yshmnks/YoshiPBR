rem Use this batch file to build YoshiPBR for Visual Studio
rmdir /s /q build
mkdir build
cd build
cmake ..
cmake --build .
start YoshiPBR.sln
