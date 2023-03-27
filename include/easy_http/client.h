#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#include <string>
#include <easy_http/request.h>

class Client {
public:
	Client(const std::string &ip, int port);
	~Client();

	//! Send new request
	Request SendRequest(Request::RequestMethod method, std::string path);

private:
	//! libevent request handler
	static void ResponseHandler(struct evhttp_request* request, void* client_ptr);

	std::string http_ip_;
	uint16_t http_port_;

	struct evhttp_request* e_last_request_;
	struct event_base* e_base_;
	struct evhttp_connection* e_conn_;
};

#endif // !_HTTP_CLIENT_H_
