#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#include <string>
#include <easy_http/request.h>

class Client {
public:
	typedef std::map<std::string, std::string> HeaderList;

	Client(const std::string &ip, int port);
	Client(const std::string &url);
	~Client();

	//! Send new request
	Request SendRequest(Request::RequestMethod method, std::string path, const HeaderList& headers = HeaderList());

	//! Send chunked request
	Request SendChunkedRequest(std::function<void(Request&)> h, Request::RequestMethod method, std::string path, const HeaderList& headers = HeaderList());

private:

	Request MakeRequest(struct evhttp_request* request, Request::RequestMethod method, std::string path, const HeaderList& headers);

	//! libevent request handler
	static void ResponseHandler(struct evhttp_request* request, void* client_ptr);
	static void ChunkedResponseHandler(struct evhttp_request* request, void* client_ptr);
	static void ResponseErrorHandler(enum evhttp_request_error err_code, void* client_ptr);

	void Init();

	std::string http_ip_;
	uint16_t http_port_;
	int error_code_;
	std::function<void(Request&)> chunk_handler_;

	struct evhttp_request* e_last_request_;
	struct event_base* e_base_;
	struct evhttp_connection* e_conn_;
};

#endif // !_HTTP_CLIENT_H_
