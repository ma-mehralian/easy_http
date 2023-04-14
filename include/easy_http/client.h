#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#include <string>
#include <easy_http/request.h>

class Client {
public:
	typedef std::map<std::string, std::string> HeaderList;
	
	//! create new connection to ip:port and keep it open
	Client(const std::string& ip, int port);
	Client(const std::string& url);

	~Client();

	//! create new request for the client connection
	Request CreateRequest(Request::RequestMethod method, std::string path, const HeaderList& headers = HeaderList());

	//! Send a new request
	Request SendRequest(Request& request);

	//! Send a new request with chuncked response
	void SendChunkedRequest(std::function<void(const Request&)> h, Request& request);

private:

	void MakeAsyncRequest(Request& request);
	Request MakeRequest(Request& request);

	//! libevent request handler
	static void ResponseHandler(struct evhttp_request* request, void* client_ptr);
	static void ChunkedResponseHandler(struct evhttp_request* request, void* client_ptr);
	static void ResponseErrorHandler(int err_code, void* client_ptr);

	void Init();

	std::string http_ip_;
	uint16_t http_port_;
	int error_code_;
	std::function<void(const Request&)> chunk_handler_;

	struct evhttp_request* e_last_request_;
	struct event_base* e_base_;
	struct evhttp_connection* e_conn_;
};

#endif // !_HTTP_CLIENT_H_
