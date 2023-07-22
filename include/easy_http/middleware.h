#ifndef _HTTP_MIDDLEWARE_H_
#define _HTTP_MIDDLEWARE_H_

#include <easy_http/response.h>
#include <easy_http/request.h>

class Middleware {
public:
	Middleware() : next_(nullptr) {	}

	virtual Response Handle(const Request& request, Response::RequestHandler next) = 0;

private:
	friend class Route;
	friend class Controller;

	Response CallHandler(const Request& request);

	Response::RequestHandler last_;
	std::unique_ptr<Middleware> next_;
	std::string url_;
};

#endif // !_HTTP_MIDDLEWARE_H_
