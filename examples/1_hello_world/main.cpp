#include <iostream>
#include <easy_http/client.h>
#include <easy_http/server.h>

using namespace std;

class MyController: public Controller {
public:
	MyController() {
		RegisterHandler("test1", Request::RequestMethod::GET,
			std::bind(&MyController::Test1, this, std::placeholders::_1));
		RegisterHandler("test2", Request::RequestMethod::POST,
			std::bind(&MyController::Test2, this, std::placeholders::_1));
	}

private:
	Response Test1(Request& request) {
		Client c("127.0.0.1", 4313);
		auto req = c.SendRequest(Request::RequestMethod::GET, "/api/color_classes");
		cout << "test1 called" << endl;
		Response response(request, 200, { {"Content-type", "application/json"} });
		response.SetContent(req.GetContent());
		return response;
	}

	Response Test2(Request& request) {
		cout << "test2 called" << endl;
		string name = request.Query<string>("name", "no-name");
		Response response(request, 200);
		response.SetContent("Hello "+ name);
		return response;
	}
};

int main(int argn, char* argc[]) {
	string ip = "0.0.0.0";
	int port = 4000;
	Server s(ip, port);
	s.RegisterController(make_unique<MyController>());
	if (s.Start() != 0){
		cout << "Cannot start HTTP server http://" << ip << ":" << port << endl;
		return -1;
	}
	return 0;
}