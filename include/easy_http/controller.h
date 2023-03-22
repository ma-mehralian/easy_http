#ifndef _HTTP_CONTROLLER_H_
#define _HTTP_CONTROLLER_H_

#include <easy_http/request.h>
#include <easy_http/response.h>
#include <easy_http/middleware.h>
#include <functional>

class Controller {
public:
	//! contructors
	Controller(std::string url_prefix = "/");
	//Controller(const Controller& controller) :
	//	url_prefix_(controller.url_prefix_), handlers_(controller.handlers_), middlewares_(controller.middlewares_) {}//

	Response Handle(Request& request);

	// check if the request will match with one of controller handlers
	bool IsMatch(Request& request);

protected:
	typedef std::function<Response(Request&)> Handler;
	typedef std::function<Response(Request&, Handler next)> MiddlewareHandler;

	//! Register new Handler using class methods
	void RegisterHandler(std::string url, Request::RequestMethod method, Handler Handler);

	//void RegisterMiddleware(MiddlewareHandler middleware);
	void RegisterMiddleware(std::unique_ptr<Middleware> middleware);

	std::string url_prefix_;

private:
	friend Middleware;
	typedef std::tuple<std::string, Request::RequestMethod, Handler> HandlerMatch;

	Response CallHandler(Request& request);
	bool IsMatch(Request& request, HandlerMatch& h);


	std::vector<HandlerMatch> handlers_;

	std::unique_ptr<Middleware> middleware_chain_;
};

#endif // !_HTTP_CONTROLLER_H_
