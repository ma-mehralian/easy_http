An easy to use http client/server based on libevent writen in c++.

cmake -G "Ninja" -DLibevent_DIR=path/to/cmake/libevent -DCMAKE_BUILD_TYPE=Release
cmake --build . --target install -- -j4
