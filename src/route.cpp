#include <easy_http/route.h>

Route Route::Get(std::string url, Response::RequestHandler handler) {
	return Route(RequestBase::RequestMethod::GET, url, handler);
}

Route Route::Post(std::string url, Response::RequestHandler handler) {
	return Route(RequestBase::RequestMethod::POST, url, handler);
}

Route Route::Put(std::string url, Response::RequestHandler handler) {
	return Route(RequestBase::RequestMethod::PUT, url, handler);
}

Route Route::Patch(std::string url, Response::RequestHandler handler) {
	return Route(RequestBase::RequestMethod::PATCH, url, handler);
}

Route Route::Delete(std::string url, Response::RequestHandler handler) {
	return Route(RequestBase::RequestMethod::DELETE, url, handler);
}

Response Route::CallHandler(const Request& request) {
	if (middleware_chain_)
		return middleware_chain_->CallHandler(request);
	else
		return this->handler_(request);
}

Route& Route::AddMiddleware(std::unique_ptr<Middleware> middleware) {
	middleware->last_ = this->handler_;
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
