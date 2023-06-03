#ifndef _HTTP_REQUEST_H_
#define _HTTP_REQUEST_H_

#include <string>
#include <vector>
#include <map>
#ifdef USE_JSON
#include <nlohmann/json.hpp>
#endif // USE_JSON

#include <functional>
#include <regex>
#include <type_traits>

struct evhttp_request;

class Request {
public:
	enum class RequestMethod { GET, POST, HEAD, PUT, DELETE, OPTIONS, TRACE, CONNECT, PATCH };
	typedef std::map<std::string, std::string> ParamList;

	//! constructor for the received request (used for Server)
	Request(evhttp_request* request);

	//! constructor for a sending request (used for Client)
	Request(evhttp_request* request, Request::RequestMethod method, std::string url);


	//copy constructor
	Request(const Request& req);

	// move constructor
	Request(Request&& req);

	Request& operator=(const Request& req);
	Request& operator=(Request&& req);

	~Request();

	Request& SetChunkCallback(std::function<bool(std::string&)> func);

	int Wait();

	void Cancel();

#ifdef USE_JSON
	//! Get the JSON payload for the request.
	nlohmann::json Json() const;
#endif // USE_JSON

	const std::string GetContent() const;

	// set string content
	Request& SetContent(const std::string& content);

	// set binary content
	Request& SetContent(const std::vector<char>& content);

	// set file content
	Request& SetFileContent(std::string file_path);

	//!Retrieve a query string item from the request.
	template <typename T>
	T Query(std::string key, T default_value = T()) const { return GetVal<T>(uri_.query, key, default_value); }

	const ParamList& Queries() const { return uri_.query; }

	//! Determine if the request contains a non-empty value for an input item.
	bool QueryFilled(std::string key) const { return IsFilled(uri_.query, key); }

	//! Push the header to sending request headers
	Request& PushHeader(const std::string& key, const std::string& value);

	//! Push multiple headers to sending request headers
	Request& PushHeader(const ParamList& headers);

	//! Retrieve a headers list from the received request.
	const ParamList& Headers() const { return input_headers_; }

	//!Retrieve a header from the received request.
	template <typename T>
	T Header(std::string key, T default_value = T()) const { return GetVal<T>(input_headers_, key, default_value); }

	//! Determine if the received request contains a non-empty value for an input item.
	bool HeaderFilled(std::string key) const { return IsFilled(input_headers_, key); }

	//! Get the client IP address.
	const std::string& ClientIp() const { return client_ip_; }

	//! Get the client port.
	int ClientPort() const { return uri_.port; } //TODO: vs client_port_

	const std::string& Host() const { return uri_.host; }

	//! Get the URL (no query string) for the request.
	const std::string& Url() const { return uri_.path; }

	//! Determine if the URL matches a given pattern (regular expression) and fill vars with regex groups value
	//! sample pattern: "/engine/([0-9]+)/"
	bool UrlIs(std::string pattern, std::vector<std::string>& vars) const;
	bool UrlIs(std::string pattern) const;

	template<typename... outs>
	std::tuple<outs...> UrlVars(std::string pattern) const {
		std::smatch base_match;
		std::regex_match(uri_.path, base_match, std::regex(pattern));
		return tuple_helper(base_match, std::tuple<outs...>());
	}

	//! Get the URL for the request with the added query string parameters
	const std::string& FullUrl() const { return uri_.full_path; }

	//! Get the request method.
	RequestMethod Method() const { return method_; }

protected:
	friend class Response;
	friend class Client;

	//! reply the received request (this function is called in Response class)
	void Reply(int status_code);

	evhttp_request* evrequest_;

private:
	enum RequestType { REQUEST, RESPONSE } type_;

	//scheme://[[userinfo]@]foo.com[:port]]/[path][?query][#fragment]
	struct Uri {
		std::string scheme;
		std::string userinfo;
		int port;
		std::string path;
		std::string full_path;
		ParamList query;
		std::string fragment;
		std::string host;
	} uri_;
	RequestMethod method_;
	std::string client_ip_;
	int client_port_;
	ParamList input_headers_;
	//-- chunck
	bool is_chunked_ = false;
	std::function<bool(std::string&)> chunk_callback_ = nullptr;

	//! fill uri_ values from input url or path
	void ParsUri(std::string url);

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

#endif // !_HTTP_REQUEST_H_
