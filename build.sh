cmake -B build -DCMAKE_INSTALL_PREFIX="./install" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build build --config Release
cmake --install build
