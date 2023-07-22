#include <iostream>
#include <easy_http/client.h>
#include <easy_http/server.h>

using namespace std;

class MyController: public Controller {
public:
	MyController(std::string url_prefix) : Controller(url_prefix) {
		Get("/test1", &MyController::Test1);
		Post("/test2", &MyController::Test2);
	}

private:
	Response Test1(const Request& request) {
		cout << "test1 called" << endl;
		Response response(request, 200);
		response.SetContent("Test1 callback");
		return move(response);
	}

	Response Test2(const Request& request) {
		cout << "test2 called" << endl;
		string name = request.Query<string>("name", "no-name");
		Response response(request, 200);
		response.SetContent("Hello "+ name);
		return move(response);
	}
};

int main(int argn, char* argc[]) {
	string ip = "0.0.0.0";
	int port = 4000;
	Server s(ip, port);
	s.RegisterController(make_unique<MyController>("/users"));
	if (s.Start() != 0){
		cout << "Cannot start HTTP server http://" << ip << ":" << port << endl;
		return -1;
	}
	return 0;
}