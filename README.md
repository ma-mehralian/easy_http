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

# Examples

## Client

```
#include <iostream>
#include <easy_http/client.h>

int main(int argn, char* argc[]) {
	Client c("http://headers.jsontest.com");
	try {
		auto req = c.Get("/",
			[](const Response& res) {
				std::cout << "response[" << res.GetStatusCode() << "]: " << res.GetContent() << std::endl;
			})
			.PushHeader("test_header", "test_value");
		std::cout << "calling: " << req.FullUrl() << std::endl;
		c.SendRequest(req);
	}
	catch (const std::exception& e) {
		std::cout << "request failed: " << e.what() << std::endl;
	}
	return 0;
}
```

## Server

```
#include <iostream>
#include <easy_http/server.h>

using namespace std;

Response Test(Request& request) {
	cout << "test called" << endl;
	Response response(request, 200);
	response.SetContent("Test callback");
	return response;
}

int main(int argn, char* argc[]) {
	string ip = "0.0.0.0";
	int port = 4000;
	Server s(ip, port);
	s.Get("/test", Test);
	if (s.Start() != 0){
		cout << "Cannot start HTTP server http://" << ip << ":" << port << endl;
		return -1;
	}
	return 0;
}
```