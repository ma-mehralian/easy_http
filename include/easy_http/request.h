#ifndef _HTTP_REQUEST_H_
#define _HTTP_REQUEST_H_

#include <easy_http/request_base.h>

class Request: public RequestBaseAbstract<Request> {
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

	//! Push the parameter to sending request parameters
	//! \note: All HTTP header keys are converted to lowercase in both directions
	//!  (since HTTP header keys are defined to be case-insensitive)
	Request& PushParam(const std::string& key, const std::string& value);

	//! Push multiple headers to sending request parameters
	//! \note: All HTTP header keys are converted to lowercase in both directions
	//!  (since HTTP header keys are defined to be case-insensitive)
	Request& PushParam(const ParamList& params);

private:
	struct evhttp_connection* e_con_;
	RequestBase::RequestMethod method_;
};

#endif // !_HTTP_REQUEST_H_
