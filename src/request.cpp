#include <easy_http/request.h>
#include <easy_http/response.h>

using namespace std;

Request::Request(struct evhttp_request* request) : RequestBaseAbstract<Request>(request){
}

Request::Request(struct evhttp_connection* e_con, RequestBase::RequestMethod method, std::string url, RequestBase::ResponseHandler handler, bool is_chunked)
	: e_con_(e_con), method_(method), 
	RequestBaseAbstract<Request>(url, handler, is_chunked){}

Request::Request(const Request& req) :RequestBaseAbstract<Request>(req) {
	*this = req;
}

Request& Request::operator=(const Request& req) {
	RequestBase::operator=(req);
	this->e_con_ = req.e_con_;
	this->method_ = req.method_;
	return *this;
}

Request::Request(Request&& req) :RequestBaseAbstract<Request>(req) {
	*this = move(req);
}

Request& Request::operator=(Request&& req) {
	RequestBase::operator=(req);
	this->e_con_ = req.e_con_;
	this->method_ = req.method_;
	return *this;
}

void Request::Send() {
	RequestBase::Send(e_con_, method_);
}

void Request::SendAsync() {
	RequestBase::SendAsync(e_con_, method_);
}
