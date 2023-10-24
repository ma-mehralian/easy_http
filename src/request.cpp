#include <easy_http/request.h>
#include <easy_http/response.h>
#include <event2/http.h>

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

Request& Request::PushParam(const std::string& key, const std::string& value) {
	return PushParam({ {key, value} });
}

Request& Request::PushParam(const ParamList& params) {
	string params_str = "";
	for (auto& p : params) {
		auto k = evhttp_uriencode(p.first.c_str(), p.first.size(), 0);
		auto v = evhttp_uriencode(p.second.c_str(), p.second.size(), 0);
		params_str += (!params_str.empty() ? "&" : "") + string(k) + "=" + string(v);
		delete k;
		delete v;
	}
	evhttp_uri_set_query(e_uri_.get(), params_str.c_str());
	return *this;
}
