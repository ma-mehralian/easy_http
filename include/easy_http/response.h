#ifndef _HTTP_RESPONSE_H_
#define _HTTP_RESPONSE_H_

#include <easy_http/request.h>
#include <string>
#include <vector>
#include <map>

//! https://github.com/symfony/http-foundation/blob/6.2/Response.php
class Response {
public:
	typedef std::map<std::string, std::string> HeaderList;
	//! constructor
	Response(Request& request, int status = 200, HeaderList headers = HeaderList());
	Response(const Response& response);
	Response(Response&& response);
	Response& operator=(const Response& response);
	Response& operator=(Response&& response);

	~Response();

	//! set response content
	Response& SetContent(std::string content);

	//! set handler for chunked response
	Response& SetChunkCallback(std::function<bool(std::string&)> func);

	//! set response headers
	Response& SetHeaders(const HeaderList& headers);

	//void SetProtocolVersion(std::string version) { version_ = version; }
	//std::string GetProtocolVersion() const { return version_; }

	Response& SetStatusCode(int code);
	int GetStatusCode() const { return status_code_; }

	//Response& SetCharset(std::string charset);
	//std::string GetCharset() const;

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

	Response& SetFilePath(std::string path);

	int Send();

protected:
	Request& request_;
	int status_code_;
};

#endif // !_HTTP_RESPONSE_H_
