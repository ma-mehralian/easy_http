#ifndef _HTTP_CONTROLLER_H_
#define _HTTP_CONTROLLER_H_

#include <easy_http/request.h>
#include <easy_http/response.h>
#include <easy_http/middleware.h>
#include <easy_http/route.h>
#include <functional>

class Controller {
public:
	//! contructors wit url prefix
	Controller(std::string url_prefix);

	// Handle request if the request matched with caontroller routes
	Response Handle(Request& request);

	// check if the request will match with one of the controller routes
	bool IsMatch(Request& request);

protected:
	typedef std::function<Response(Request&)> Handler;

	//! Add new Get route
	Route& Get(std::string url, Handler handler);

	//! Add new Post route
	Route& Post(std::string url, Handler handler);

	//! Add new Put route
	Route& Put(std::string url, Handler handler);

	//! Add new Patch route
	Route& Patch(std::string url, Handler handler);

	//! Add new Delete route
	Route& Delete(std::string url, Handler handler);

	Controller& AddMiddleware(std::unique_ptr<Middleware> middleware);

	std::string url_prefix_;

private:
	Response CallHandler(Request& request);

	std::vector<Route> routes_;

	std::unique_ptr<Middleware> middleware_chain_;
};

#endif // !_HTTP_CONTROLLER_H_
