#ifndef _HTTP_REQUEST_BASE_H_
#define _HTTP_REQUEST_BASE_H_

#include <functional>
#include <map>
#include <regex>
#include <type_traits>

//template <typename DT> 
class RequestBase {
public:
	enum class RequestMethod { GET, POST, HEAD, PUT, DELETE, OPTIONS, TRACE, CONNECT, PATCH };
	typedef std::map<std::string, std::string> ParamList;
	typedef std::function<void(const RequestBase&)> Handler;
	typedef std::function<void(const class Response&)> ResponseHandler;
	typedef std::function<class Response (const class Request&)> RequestHandler;

protected:
	//! server usage: constructor for receiving response
	RequestBase(struct evhttp_request* request);

	//! client usage: constructor for sending request
	RequestBase(std::string url, ResponseHandler handler, bool is_chunked);

public:
	//! copy constructor
	RequestBase(const RequestBase& req);
	RequestBase& operator=(const RequestBase& req);

	//! move constructor
	RequestBase(RequestBase&& req);
	RequestBase& operator=(RequestBase&& req);

	~RequestBase();

	// get string content
	const std::string GetContent() const;

	// set string content
	RequestBase& SetContent(const std::string& content);

	// set binary content
	RequestBase& SetContent(const std::vector<char>& content);

	// set file content
	RequestBase& SetFileContent(std::string file_path);

	//! Retrieve a query string item from the request.
	template <typename T>
	T Query(std::string key, T default_value = T()) const { return GetVal<T>(queries_, key, default_value); }

	const ParamList Queries() const;

	//! Determine if the request contains a non-empty value for an input item.
	bool QueryFilled(std::string key) const { return IsFilled(queries_, key); }

	//! Push the header to sending request headers
	//! \note: All HTTP header keys are converted to lowercase in both directions
	//!  (since HTTP header keys are defined to be case-insensitive)
	RequestBase& PushHeader(const std::string& key, const std::string& value);

	//! Push multiple headers to sending request headers
	//! \note: All HTTP header keys are converted to lowercase in both directions
	//!  (since HTTP header keys are defined to be case-insensitive)
	RequestBase& PushHeader(const ParamList& headers);

	//! Retrieve a headers list from the received request.
	const ParamList Headers() const;

	//!Retrieve a header from the received request.
	//! \note: All HTTP header keys are converted to lowercase in both directions
	//!  (since HTTP header keys are defined to be case-insensitive)
	template <typename T>
	T Header(std::string key, T default_value = T()) const { return GetVal<T>(input_headers_, ToLower(key), default_value); }

	//! Determine if the received request contains a non-empty value for an input item.
	bool HeaderFilled(std::string key) const { return IsFilled(input_headers_, ToLower(key)); }

	//! Determine if the URL matches a given pattern (regular expression) and fill vars with regex groups value
	//! sample pattern: "/user/([0-9]+)/"
	bool UrlIs(std::string pattern, std::vector<std::string>& vars) const;
	bool UrlIs(std::string pattern) const;

	template<typename... outs>
	std::tuple<outs...> UrlVars(std::string pattern) const {
		std::smatch base_match;
		string path = this->Path();
		std::regex_match(path, base_match, std::regex(pattern));
		return tuple_helper(base_match, std::tuple<outs...>());
	}

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

	//! Get response code from the request URL
	const int ResponseCode() const;

	//! Get the connection port.
	const int ConnectionPort() const;

	//! Get the connection address.
	const std::string ConnectionAddress() const;

	RequestBase& SetChunkCallback(std::function<bool(std::string&)> func);
	void Cancel();
	int Wait();
	bool IsComplete();

protected:
	//! reply the received request (this function is called in Response class)
	void Reply(int status_code);

	//! send the maked request (this function is called for client requests)
	void SendAsync(struct evhttp_connection* e_con, RequestMethod method);
	void Send(struct evhttp_connection* e_con, RequestMethod method);

private:
	enum RequestType { REQUEST, RESPONSE } type_;
	static const std::map<std::string, std::string> content_types_;
	struct evhttp_request* e_request_;
	std::shared_ptr<struct evhttp_uri> e_uri_;
	std::shared_ptr<struct CallBackContainer> cb_container_;
	ResponseHandler response_handler_;
	bool request_complete_;
	bool is_chunked_ = false;
	std::function<bool(std::string&)> chunk_callback_ = nullptr;

	ParamList queries_;
	ParamList input_headers_;

	//! change string to lowercase
	std::string ToLower(const std::string& str) const;

	//static void OnReplyComplete(struct evhttp_request* request, void* arg);

	//static void ResponseHandle(evhttp_request* request, void* request_ptr);

	//static void ResponseChunkedHandle(evhttp_request* request, void* request_ptr);

	//static void ResponseErrorHandle(enum evhttp_request_error err_code, void* request_ptr);

	//! check if key exist in list and is not empty
	bool IsFilled(const std::map<std::string, std::string>& list, std::string key) const;

	template <typename T>
	T GetVal(std::string str_value) const;

	//! Retrieve value from the list and return defult value if key not exist or value is empty.
	template <typename T>
	T GetVal(const std::map<std::string, std::string>& list, std::string key, T default_value) const {
		try {
			return IsFilled(list, key) ? GetVal<T>(list.at(key)) : default_value;
		}
		catch (...) {
			return default_value;
		}
	}

	template<typename T>
	std::tuple<T> tuple_helper(const std::smatch& matches, int index, std::tuple<T> t) const {
		return { GetVal<T>(matches[index].str()) };
	}

	template<typename Head, typename ...Tail>
	std::tuple<Head, Tail...> tuple_helper(const std::smatch& matches, int index, std::tuple<Head, Tail...> t) const {
		return std::tuple_cat(
			std::make_tuple(GetVal<Head>(matches[index].str())),
			tuple_helper(matches, index + 1, std::tuple<Tail...>()));
	}

	template<typename Head, typename ...Tail>
	std::tuple<Head, Tail...> tuple_helper(const std::smatch& matches, std::tuple<Head, Tail...> t) const {
		return tuple_helper(matches, 1, t);
	}


};

#endif // !_HTTP_REQUEST_BASE_H_
