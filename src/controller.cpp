#include <easy_http/controller.h>
#include <easy_http/middleware.h>
#include <functional>

using namespace std;
	
Controller::Controller(std::string url_prefix) : url_prefix_(url_prefix), middleware_chain_(nullptr) {
	if (url_prefix[0] != '/')
		throw std::runtime_error("url prefix should be started with '/'");
}

Response Controller::Handle(const Request& request) {
	for (auto& r : routes_)
		if (request.UrlIs(r.Url()) && request.Method() == r.Method())
			return r.CallHandler(request);
	return Response(request, 404);
}

bool Controller::IsMatch(const Request& request) const {
	for (auto& r : routes_)
		if (r.IsMatch(request))
			return true;
	return false;
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

Response Controller::CallHandler(const Request& request) {
	if (middleware_chain_)
		return middleware_chain_->CallHandler(request);
	else
		return this->Handle(request);
}
