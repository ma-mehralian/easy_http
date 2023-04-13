#include <easy_http/controller.h>
#include <easy_http/middleware.h>
#include <functional>

using namespace std;
	
Controller::Controller(std::string url_prefix) : url_prefix_(url_prefix) {
	if (url_prefix[0] != '/')
		throw std::runtime_error("url prefix should be started with '/'");
}

Response Controller::Handle(Request& request) {
	for (auto& r : routes_)
		if (request.UrlIs(r.Url()))
			r.CallHandler(request);
	return Response(request, 404);
}

bool Controller::IsMatch(Request& request) {
	for (auto& r : routes_)
		if (request.UrlIs(r.Url()))
			return true;
	return false;
}

Route& Controller::Get(std::string url, Handler handler) {
	routes_.push_back(Route::Get(url_prefix_ + url, handler));
	return routes_.back();
}

Route& Controller::Post(std::string url, Handler handler) {
	routes_.push_back(Route::Post(url_prefix_ + url, handler));
	return routes_.back();
}

Route& Controller::Put(std::string url, Handler handler) {
	routes_.push_back(Route::Put(url_prefix_ + url, handler));
	return routes_.back();
}

Route& Controller::Patch(std::string url, Handler handler) {
	routes_.push_back(Route::Patch(url_prefix_ + url, handler));
	return routes_.back();
}

Route& Controller::Delete(std::string url, Handler handler) {
	routes_.push_back(Route::Delete(url_prefix_ + url, handler));
	return routes_.back();
}
 
Controller& Controller::AddMiddleware(std::unique_ptr<Middleware> middleware) {
	middleware->last_ = bind(&Controller::Handle, this, placeholders::_1);
	if (!middleware_chain_)
		middleware_chain_ = std::move(middleware);
	else { // add new middleware to the end of list
		auto last = middleware_chain_.get();
		while (last->next_ != nullptr)
			last = last->next_.get();
		last->next_ = std::move(middleware);
	}
	return *this;
}

Response Controller::CallHandler(Request& request) {
	if (middleware_chain_)
		return middleware_chain_->CallHandler(request);
	else
		return this->CallHandler(request);
}
