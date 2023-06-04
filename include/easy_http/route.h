#ifndef _HTTP_ROUTE_H_
#define _HTTP_ROUTE_H_

#include <easy_http/request.h>
#include <easy_http/response.h>
#include <easy_http/middleware.h>
#include <functional>

class Route {
public:
	typedef std::function<Response(Request&)> Handler;

	static Route Get(std::string url, Handler handler);

	static Route Post(std::string url, Handler handler);

	static Route Put(std::string url, Handler handler);

	static Route Patch(std::string url, Handler handler);

	static Route Delete(std::string url, Handler handler);

	Response CallHandler(Request& request);

	Route& AddMiddleware(std::unique_ptr<Middleware> middleware);

	const std::string Url() const { return url_; }

	const Request::RequestMethod Method() const { return method_; }

	template<class T>
	std::enable_if_t<std::is_base_of<Middleware, T>::value, T*>
	GetMiddlewareByType() {
		if (!middleware_chain_) return nullptr;
		auto last = middleware_chain_.get();
		while (last != nullptr && typeid(T) != typeid(*last))
			last = last->next_.get();
		return static_cast<T*>(last);
	}

private:
	Route(Request::RequestMethod method, std::string url, Handler handler)
		: url_(url), method_(method), handler_(handler), middleware_chain_(nullptr) {}

	std::string url_;

	Request::RequestMethod method_;

	Handler handler_;

	std::unique_ptr<Middleware> middleware_chain_;

};
#endif // !_HTTP_ROUTE_H_
