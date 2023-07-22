#ifndef _HTTP_REQUEST_H_
#define _HTTP_REQUEST_H_

#include <easy_http/request_base.h>

class Request: public RequestBase {
public:
	//! server constructor
	Request(struct evhttp_request* request);

	//! client constructor
	Request(struct evhttp_connection* e_con, RequestBase::RequestMethod method, std::string url, RequestBase::ResponseHandler handler, bool is_chunked = false);

	//! copy constructor
	Request(const Request& req);
	Request& operator=(const Request& req);

	//! move constructor
	Request(Request&& req);
	Request& operator=(Request&& req);

	void Send();
	void SendAsync();

private:
	struct evhttp_connection* e_con_;
	RequestBase::RequestMethod method_;
};

#endif // !_HTTP_REQUEST_H_
