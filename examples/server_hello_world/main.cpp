#include <iostream>
#include <easy_http/server.h>

using namespace std;

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

int main(int argn, char* argc[]) {
	string ip = "0.0.0.0";
	int port = 4000;
	Server s(ip, port);
	s.Get("/test1", Test1);
	s.Post("/test2", Test2);
	if (s.Start() != 0){
		cout << "Cannot start HTTP server http://" << ip << ":" << port << endl;
		return -1;
	}
	return 0;
}