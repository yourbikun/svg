cmake -B build -DCMAKE_INSTALL_PREFIX="./install" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
cmake --build build --config Release

cmake --install build
