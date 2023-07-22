#ifndef _HTTP_EV_REQUEST_H_
#define _HTTP_EV_REQUEST_H_

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

struct evhttp_request;

/*!
* C++ wrapper for LibEvent 2.
* 
*	https://bob:bobby@www.example.com:8080/file;p=1?q=2#third
*	\___/   \_/ \___/ \_____________/ \__/\_______/ \_/ \___/
*	  |      |    |          |         |      | \_/  |    |
*	Scheme User Password    Host      Port  Path |   | Fragment
*			\____________________________/       | Query
*						   |              Path parameter
*					   Authority
*/
class EvRequest {
public:
	enum class RequestMethod { GET, POST, HEAD, PUT, DELETE, OPTIONS, TRACE, CONNECT, PATCH };
	typedef std::map<std::string, std::string> ParamList;
	typedef std::function<void(std::unique_ptr<EvRequest>)> Handler;

	//! constructor for the received request (used for Server)
	EvRequest(evhttp_request* request);

	//! constructor for a sending request (used for Client)
	EvRequest(std::string url, Handler handler, bool is_chunked = false);

	//! copy constructor
	EvRequest(const EvRequest& req) = delete;
	EvRequest& operator=(const EvRequest& req) = delete;

	//! move constructor
	EvRequest(EvRequest&& req);
	EvRequest& operator=(EvRequest&& req);

	//! deconstructor
	~EvRequest();



	// get string content
	const std::string GetContent() const;

	// set string content
	EvRequest& SetContent(const std::string& content);

	// set binary content
	EvRequest& SetContent(const std::vector<char>& content);

	// set file content
	EvRequest& SetFileContent(std::string file_path);

	//! Push the header to sending request headers
	//! \note: All HTTP header keys are converted to lowercase in both directions
	//!  (since HTTP header keys are defined to be case-insensitive)
	EvRequest& PushHeader(const std::string& key, const std::string& value);

	//! Get the request method.
	const RequestMethod Method() const;

	//! Get the URL from the request including query string parameters
	const std::string FullUrl() const;

	//! Get scheme from the request URL
	const std::string Scheme() const;

	//! Get user info from the request URL
	const std::string User() const;

	//! Get host from the request URL
	const std::string Host() const;

	//! Get port from the request URL
	const int Port() const;

	//! Get path from the request URL
	const std::string Path() const;

	//! Retrieve list of query parameters from the request url
	const ParamList Queries() const;

	//! Retrieve headers list from the received request
	const ParamList Headers() const;

	//! Get port from the request URL
	const int ResponseCode() const;

	//! Get the connection port.
	const int ConnectionPort() const;

	//! Get the connection address.
	const std::string ConnectionAddress() const;

	EvRequest& SetChunkCallback(std::function<bool(std::string&)> func);
	void Cancel();
	int Wait();
	bool Ongoing();


	//! reply the received request (this function is called in Response class)
	void Reply(int status_code);

	//! send the maked request (this function is called for client requests)
	void SendAsync(struct evhttp_connection* e_con, EvRequest::RequestMethod method);
	void Send(struct evhttp_connection* e_con, EvRequest::RequestMethod method);


private:
	enum RequestType { REQUEST, RESPONSE } type_;
	evhttp_request* e_request_;
	struct evhttp_uri* e_uri_;
	Handler response_handler_;
	bool request_complete_;
	bool is_chunked_ = false;
	std::function<bool(std::string&)> chunk_callback_ = nullptr;
	static const std::map<std::string, std::string> content_types_;


	//! change string to lowercase
	std::string ToLower(const std::string& str) const;

	static void OnReplyComplete(struct evhttp_request* request, void* arg);

	static void ResponseHandler(evhttp_request* request, void* request_ptr);

	static void ResponseChunkedHandler(evhttp_request* request, void* request_ptr);

	static void ResponseErrorHandler(enum evhttp_request_error err_code, void* request_ptr);
};

#endif // !_HTTP_EV_REQUEST_H_
