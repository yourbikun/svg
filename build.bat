cmake -B build -DCMAKE_INSTALL_PREFIX="./install" -DCMAKE_BUILD_TYPE=Release -DCMAKE_GENERATOR_TOOLSET=v142 -A Win32 -DCMAKE_SYSTEM_VERSION=8.1 -DBUILD_SHARED_LIBS=OFF
cmake --build build --config Release
cmake --install build
