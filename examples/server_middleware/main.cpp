#include <iostream>
#include <easy_http/server.h>

using namespace std;

class AuthenticateMiddleware : public Middleware {
	const string token_;

public:
	AuthenticateMiddleware(std::string token) :token_(token) {}

	Response Handle(Request& request, Handler next) override {
		cout << "AuthenticateMiddleware" << endl;
		if (request.Header<string>("Authorization") == token_)
			return next(request);
		else
			return Response(request, 401);
	}
};

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
	s.Get("/test1", Test1)
		.AddMiddleware(make_unique<AuthenticateMiddleware>("C194A94D-BEB5-4A8D-B4BB-94E99AA38D61"));
	s.Post("/test2", Test2);
	if (s.Start() != 0){
		cout << "Cannot start HTTP server http://" << ip << ":" << port << endl;
		return -1;
	}
	return 0;
}