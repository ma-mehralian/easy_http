#include <iostream>
#include <easy_http/server.h>
#ifdef USE_MLD
#include <vld.h>
#endif

using namespace std;

class AuthenticateMiddleware : public Middleware {
	const string token_;

public:
	AuthenticateMiddleware(std::string token) :token_(token) {}

	Response Handle(const Request& request, RequestBase::RequestHandler next) override {
		if (request.Header<string>("Authorization") == token_)
			return next(request);
		else {
			auto r = Response(request, 401);
			r.SetContent("authentication failed");
			return move(r);
		}
	}
	auto GetToken() { return token_; }
};

class MyController : public Controller {
public:
	MyController(std::string url_prefix) : Controller(url_prefix) {
		Get("/test1", &MyController::Test1);
		Post("/test2", &MyController::Test2);
		AddMiddleware(make_unique<AuthenticateMiddleware>("AbCd@1234"));
	}

private:
	Response Test1(const Request& request) {
		cout << "test1 called" << endl;
		Response response(request, 200);
		response.SetContent("Test1 callback");
		return move(response);
	}

	Response Test2(const Request& request) {
		cout << "test2 called with token " 
			<< this->GetMiddlewareByType<AuthenticateMiddleware>()->GetToken() 
			<< endl;
		string name = request.Query<string>("name", "no-name");
		Response response(request, 200);
		response.SetContent("Hello " + name);
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