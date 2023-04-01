#include <iostream>
#include <easy_http/server.h>

using namespace std;

class MyServer: public Server {
public:
	using Server::Server;
	Response RequestHandler(Request& request) override {
		if (request.Method() == Request::RequestMethod::GET && request.UrlIs("/test1"))
			return Test1(request);
		else if (request.Method() == Request::RequestMethod::POST && request.UrlIs("/test2"))
			return Test2(request);
		else
			return Er404(request);
	}

private:
	Response Test1(Request& request) {
		cout << "test1 called" << endl;
		Response response(request, 200);
		response.SetContent("Test1 callback");
		return response;
	}

	Response Test2(Request& request) {
		cout << "test2 called" << endl;
		string name = request.Query<string>("name", "no-name");
		Response response(request, 200);
		response.SetContent("Hello " + name);
		return response;
	}

	Response Er404(Request& request) {
		cout << "404 called" << endl;
		Response response(request, 404);
		response.SetContent("Page not found");
		return response;
	}
};

int main(int argn, char* argc[]) {
	string ip = "0.0.0.0";
	int port = 4000;
	MyServer s(ip, port);
	if (s.Start() != 0){
		cout << "Cannot start HTTP server http://" << ip << ":" << port << endl;
		return -1;
	}
	return 0;
}