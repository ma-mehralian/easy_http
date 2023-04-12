An easy to use http client/server based on libevent writen in c++.

## Bulid from source

## Step 1: build libevent

```
mkdir libevent
cd libevent
git clone https://github.com/libevent/libevent.git src --depth 1
mkdir build
cd build
cmake ../src -G "Ninja" -DEVENT__DISABLE_OPENSSL=ON -DEVENT__DISABLE_MBEDTLS=ON -DEVENT__LIBRARY_TYPE=STATIC -DCMAKE_INSTALL_PREFIX=../install -DCMAKE_BUILD_TYPE=Release
cmake --build . --target install -- -j4
```

## Step 2: build easy_http

```
mkdir easy_http
cd easy_http
git clone https://github.com/ma-mehralian/easy_http.git src
mkdir build
cd build
cmake ../src -G "Ninja" -DLibevent_DIR=path/to/cmake/libevent -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../install
cmake --build . --target install -- -j4
```