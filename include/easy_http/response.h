#ifndef _HTTP_RESPONSE_H_
#define _HTTP_RESPONSE_H_

#include <easy_http/request.h>
#include <string>
#include <vector>
#include <map>
#include <functional>

//! https://github.com/symfony/http-foundation/blob/6.2/Response.php
class Response {
public:
	typedef std::map<std::string, std::string> HeaderList;
	//! constructor
	Response(Request& request, int status = 200, HeaderList headers = HeaderList());
	Response(const Response& response);
	Response& operator=(const Response& response);

	~Response();

	void SetContent(std::string content);
	std::string GetContent() const;

	//void PushHeader(std::string key, std::string value);
	const HeaderList& GetHeaders() const;

	//void SetProtocolVersion(std::string version) { version_ = version; }
	//std::string GetProtocolVersion() const { return version_; }

	void SetStatusCode(int code) { status_code_ = code; }
	int GetStatusCode() const { return status_code_; }

	void SetCharset(std::string charset) { charset_ = charset; }
	std::string GetCharset() const { return charset_; }

	bool IsInvalid() const { return status_code_ < 100 || status_code_ >= 600; }
	bool IsInformational() const { return status_code_ >= 100 && status_code_ < 200; }
	bool IsSuccessful() const { return status_code_ >= 200 && status_code_ < 300; }
	bool IsRedirection() const { return status_code_ >= 300 && status_code_ < 400; }
	bool IsClientError() const { return status_code_ >= 400 && status_code_ < 500; }
	bool IsServerError() const { return status_code_ >= 500 && status_code_ < 600; }
	bool IsOk() const { return status_code_ == 200; }
	bool IsUnauthorized() const { return status_code_ == 401; }
	bool IsForbidden() const { return status_code_ == 403; }
	bool IsRedirect() const { throw std::exception("Not Implemented"); }
	bool IsEmpty() const { throw std::exception("Not Implemented"); }

	void SetFilePath(std::string path);

	void SetChunkCallback(std::function<bool(std::string&)> func);

	int Send();

protected:
	Request& request_;
	HeaderList headers_;
	//std::string version_;
	int status_code_;
	std::string charset_;
	bool is_file_ = false;
	//-- chunck
	bool is_chunked_ = false;
	std::function<bool(std::string&)> chunk_callback_ = nullptr;

};

#endif // !_HTTP_RESPONSE_H_
