#include <easy_http/response.h>

using namespace std;

Response::Response(struct evhttp_request* request)
	: RequestBaseAbstract<Response>(request), status_code_(RequestBase::ResponseCode())
{
}

Response::Response(const RequestBase& req, int status_code) 
	:RequestBaseAbstract<Response>(req), status_code_(status_code)
{
}

Response::~Response() {

}

Response& Response::SetStatusCode(int code) {
	status_code_ = code;
	return *this;
}

int Response::Send() {
	RequestBase::Reply(status_code_);
	return 0;
}
