#include <easy_http/response.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <event2/event.h>

using namespace std;

Response::Response(Request& request, int status, HeaderList headers)
	: request_(request), status_code_(status), headers_(headers)
{}

Response::Response(const Response& response) : request_(response.request_) {
	this->operator=(response);
}

Response& Response::operator=(const Response& response) {
	request_ = response.request_;
	headers_ = response.headers_;
	status_code_ = response.status_code_;
	return *this;
}

Response::~Response() {
}

void Response::SetContent(std::string content) {
	request_.SetContent(content);
}

const Response::HeaderList& Response::GetHeaders() const {
	return headers_;
}

void Response::SetFilePath(std::string path) {
	request_.SetFileContent(path);
}

int Response::Send() {
	request_.Reply(status_code_, headers_);
	return 0;
}
