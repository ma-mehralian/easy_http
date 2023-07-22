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
	virtual ~Controller() {}


	// Handle request if the request matched with controller routes
	Response CallHandler(const Request& request);

	// check if the request will match with one of the controller routes
	bool IsMatch(const Request& request) const;

protected:
	//! Add new Get route
	template<class Derived>
	std::enable_if_t<std::is_base_of<Controller, Derived>::value, Route&>
	Get(std::string url, Response(Derived::* handler)(const Request&)) {
		RequestBase::RequestHandler h = std::bind(handler, dynamic_cast<Derived*>(this), std::placeholders::_1);
		routes_.push_back(Route::Get(url_prefix_ + url, h));
		return routes_.back();
	}

	//! Add new Post route
	template<class Derived>
	std::enable_if_t<std::is_base_of<Controller, Derived>::value, Route&>
	Post(std::string url, Response(Derived::* handler)(const Request&)) {
		RequestBase::RequestHandler h = std::bind(handler, dynamic_cast<Derived*>(this), std::placeholders::_1);
		routes_.push_back(Route::Post(url_prefix_ + url, h));
		return routes_.back();
	}

	//! Add new Put route
	template<class Derived>
	std::enable_if_t<std::is_base_of<Controller, Derived>::value, Route&>
	Put(std::string url, Response(Derived::* handler)(const Request&)) {
		RequestBase::RequestHandler h = std::bind(handler, dynamic_cast<Derived*>(this), std::placeholders::_1);
		routes_.push_back(Route::Put(url_prefix_ + url, h));
		return routes_.back();
	}

	//! Add new Patch route
	template<class Derived>
	std::enable_if_t<std::is_base_of<Controller, Derived>::value, Route&>
	Patch(std::string url, Response(Derived::* handler)(const Request&)) {
		RequestBase::RequestHandler h = std::bind(handler, dynamic_cast<Derived*>(this), std::placeholders::_1);
		routes_.push_back(Route::Patch(url_prefix_ + url, h));
		return routes_.back();
	}

	//! Add new Delete route
	template<class Derived>
	std::enable_if_t<std::is_base_of<Controller, Derived>::value, Route&>
	Delete(std::string url, Response(Derived::* handler)(const Request&)) {
		RequestBase::RequestHandler h = std::bind(handler, dynamic_cast<Derived*>(this), std::placeholders::_1);
		routes_.push_back(Route::Delete(url_prefix_ + url, h));
		return routes_.back();
	}

	Controller& AddMiddleware(std::unique_ptr<Middleware> middleware);

	template<class T>
	std::enable_if_t<std::is_base_of<Middleware, T>::value, T*>
		GetMiddlewareByType() {
		if (!middleware_chain_) return nullptr;
		auto last = middleware_chain_.get();
		while (last != nullptr && typeid(T) != typeid(*last))
			last = last->next_.get();
		return static_cast<T*>(last);
	}

	std::string url_prefix_;

private:
	Response Handle(const Request& request);

	std::vector<Route> routes_;

	std::unique_ptr<Middleware> middleware_chain_;
};

#endif // !_HTTP_CONTROLLER_H_
