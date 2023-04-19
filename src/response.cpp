#include <easy_http/response.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <event2/event.h>

using namespace std;

Response::Response(Request& request, int status, HeaderList headers): request_(request){
	SetStatusCode(status).SetHeaders(headers);
}

Response::Response(const Response& response) : request_(response.request_) {
	status_code_ = response.status_code_;
}

Response::Response(Response&& response): request_(std::move(response.request_)) {
	status_code_ = response.status_code_;
}

Response& Response::operator=(const Response& response) {
	if (this == &response)
		return *this;

	request_ = response.request_;
	status_code_ = response.status_code_;
	return *this;
}

Response& Response::operator=(Response&& response) {
	if (this == &response)
		return *this;

	request_ = std::move(response.request_);
	status_code_ = response.status_code_;
	return *this;
}

Response::~Response() {
}

const std::string Response::GetContent() const {
	return request_.GetContent();
}

Response& Response::SetContent(const std::string content) {
	request_.SetContent(content);
	return *this; 
}

Response& Response::SetChunkCallback(std::function<bool(std::string&)> func) { 
	request_.SetChunkCallback(func); 
	return *this; 
}

Response& Response::SetHeaders(const HeaderList& headers) { 
	request_.SetHeaders(headers);
	return *this; 
}

Response& Response::SetFilePath(std::string path) {
	request_.SetFileContent(path);
	return *this; 
}

Response& Response::SetStatusCode(int code) {
	status_code_ = code; 
	return *this; 
}

int Response::Send() {
	request_.Reply(status_code_);
	return 0;
}
