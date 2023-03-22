#ifndef _HTTP_MIDDLEWARE_H_
#define _HTTP_MIDDLEWARE_H_

#include <easy_http/response.h>
#include <easy_http/request.h>

class Controller;

class Middleware {
public:
	Middleware() : next_(nullptr) {	}

	typedef std::function<Response(Request&)> Handler;

	virtual Response Handle(Request& request, Handler next) = 0;

private:
	friend Controller;
	Response CallHandler(Request& request);

	std::unique_ptr<Middleware> next_;
	Controller* controller_;
};

#endif // !_HTTP_MIDDLEWARE_H_
