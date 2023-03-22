#include <easy_http/controller.h>
#include <easy_http/middleware.h>

using namespace std;
	
Controller::Controller(std::string url_prefix) : url_prefix_(url_prefix) {
	if (url_prefix[0] != '/')
		throw std::exception("url prefix should be started with '/'");
	middleware_chain_ = nullptr;
}

Response Controller::Handle(Request& request) {
	if (middleware_chain_)
		return middleware_chain_->CallHandler(request);
	else
		return this->CallHandler(request);
}

void Controller::RegisterHandler(std::string url, Request::RequestMethod method, Handler handler){
	handlers_.push_back({url, method, handler});
}

//void Controller::RegisterMiddleware(MiddlewareHandler middleware) {
//	auto m = make_unique<Middleware>();
//	m->controller_ = this;
//	if (!middleware_chain_)
//		middleware_chain_ = std::move(m);
//	else { // add new middleware to the end of list
//		auto last = middleware_chain_.get();
//		while (last->next_ != nullptr)
//			last = last->next_.get();
//		last->next_ = std::move(middleware);
//	}
//}
 
void Controller::RegisterMiddleware(std::unique_ptr<Middleware> middleware) {
	middleware->controller_ = this;
	if (!middleware_chain_)
		middleware_chain_ = std::move(middleware);
	else { // add new middleware to the end of list
		auto last = middleware_chain_.get();
		while (last->next_ != nullptr)
			last = last->next_.get();
		last->next_ = std::move(middleware);
	}
}

Response Controller::CallHandler(Request& request) {
	for (auto& h : handlers_)
		if (IsMatch(request, h)) {
			//std::vector<string> vars;
			//request.UrlIs(url_prefix_ + std::get<0>(h), vars);
			return std::get<2>(h)(request);
		}
}

bool Controller::IsMatch(Request& request) {
	for (auto& h : handlers_)
		if (IsMatch(request, h))
			return true;
	return false;
}

bool Controller::IsMatch(Request& request, HandlerMatch& h) {
	return request.UrlIs(url_prefix_ + std::get<0>(h)) &&
		request.Method() == std::get<1>(h);
}

