An easy to use http client/server based on libevent writen in c++.

## Bulid from source

```
mkdir easy_http
cd easy_http
git clone https://github.com/ma-mehralian/easy_http.git src
mkdir build
cd build
cmake ../src -G "Ninja" -DLibevent_DIR=path/to/cmake/libevent -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../install
cmake --build . --target install -- -j4
```