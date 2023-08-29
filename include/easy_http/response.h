#ifndef _HTTP_RESPONSE_H_
#define _HTTP_RESPONSE_H_

#include <easy_http/request_base.h>
#include <string>
#include <vector>
#include <map>

/*! 
* Response for requests reply
* 
* inspired by : https://github.com/symfony/http-foundation/blob/6.2/Response.php
*/
class Response : public RequestBaseAbstract<Response> {
public:
	//! client constructor.
	//! used when received the reply from the sent request
	Response(struct evhttp_request* request);

	//! server constructor.
	//! used when replying the received request
	Response(const RequestBase& req, int status_code);

	using RequestBase::operator=;

	~Response();

	Response& SetStatusCode(int code);
	int GetStatusCode() const { return status_code_; }
	int Send();

	bool IsInvalid() const { return status_code_ < 100 || status_code_ >= 600; }
	bool IsInformational() const { return status_code_ >= 100 && status_code_ < 200; }
	bool IsSuccessful() const { return status_code_ >= 200 && status_code_ < 300; }
	bool IsRedirection() const { return status_code_ >= 300 && status_code_ < 400; }
	bool IsClientError() const { return status_code_ >= 400 && status_code_ < 500; }
	bool IsServerError() const { return status_code_ >= 500 && status_code_ < 600; }
	bool IsOk() const { return status_code_ == 200; }
	bool IsUnauthorized() const { return status_code_ == 401; }
	bool IsForbidden() const { return status_code_ == 403; }
	bool IsRedirect() const { throw std::runtime_error("Not Implemented"); }
	bool IsEmpty() const { throw std::runtime_error("Not Implemented"); }

protected:
	int status_code_;
};

#endif // !_HTTP_RESPONSE_H_
