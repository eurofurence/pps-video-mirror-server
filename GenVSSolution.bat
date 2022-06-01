SET SOURCE_DIR=.
SET GENERATOR=Visual Studio 17 2022

SET BINARIES_DIR="./bin/win/x64"
cmake CMakeLists.txt -G "%GENERATOR%" -A x64 -S %SOURCE_DIR% -B %BINARIES_DIR% -DCMAKE_TOOLCHAIN_FILE=E:/vcpkg/scripts/buildsystems/vcpkg.cmake
