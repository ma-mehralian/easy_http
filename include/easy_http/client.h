#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#include <string>
#include <easy_http/request.h>

class Client {
public:
	typedef std::map<std::string, std::string> HeaderList;

	//! create new connection to host and keep it open
	Client(const std::string& ip, int port);
	Client(const std::string& host);

	~Client();

	Request Get(std::string path, Request::ResponseHandler handler, bool is_chunked = false);

	Request Post(std::string path, Request::ResponseHandler handler, bool is_chunked = false);

	Request Put(std::string path, Request::ResponseHandler handler, bool is_chunked = false);

	Request Patch(std::string path, Request::ResponseHandler handler, bool is_chunked = false);

	Request Delete(std::string path, Request::ResponseHandler handler, bool is_chunked = false);
private:

	//! create new request for the client connection
	Request CreateRequest(RequestBase::RequestMethod method, std::string path, RequestBase::ResponseHandler handler, bool is_chunked);

	void Init();

	uint16_t http_port_;
	std::string http_scheme_;
	std::string http_host_;

	struct event_base* e_base_;
	struct evdns_base* e_dns_;
	struct evhttp_connection* e_conn_;
};

#endif // !_HTTP_CLIENT_H_
