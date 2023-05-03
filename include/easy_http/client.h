#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#include <string>
#include <easy_http/request.h>
#include <easy_http/response.h>

class Client {
public:
	typedef std::map<std::string, std::string> HeaderList;
	typedef std::function<void(const Response&)> Handler;

	//! create new connection to ip:port and keep it open
	Client(const std::string& ip, int port);
	Client(const std::string& url);

	~Client();

	Request Get(std::string path, Handler handler, bool is_chunked = false);

	Request Post(std::string path, Handler handler, bool is_chunked = false);

	Request Put(std::string path, Handler handler, bool is_chunked = false);

	Request Patch(std::string path, Handler handler, bool is_chunked = false);

	Request Delete(std::string path, Handler handler, bool is_chunked = false);

	//! send non-blocking request
	void SendAsyncRequest(Request& request);

	//! Send a new request and block until the response or error received
	void SendRequest(Request& request);

private:

	//! create new request for the client connection
	Request CreateRequest(Request::RequestMethod method, std::string path, Handler handler, bool is_chunked);

	void Init();

	std::string http_ip_;
	uint16_t http_port_;
	std::string http_scheme_;
	int error_code_;

	struct evhttp_request* e_last_request_;
	struct event_base* e_base_;
	struct evhttp_connection* e_conn_;
};

#endif // !_HTTP_CLIENT_H_
