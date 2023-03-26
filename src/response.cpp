#include <easy_http/response.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <event2/event.h>
#include <filesystem>
#include "utils.h"

using namespace std;

const std::map<string, string> content_type_table = {
	{ "", "text/html" },
	{ ".html", "text/html" },
	{ ".htm", "text/htm" },
	{ ".txt", "text/plain" },
	{ ".css", "text/css" },
	{ ".gif", "image/gif" },
	{ ".jpg", "image/jpeg" },
	{ ".jpeg", "image/jpeg" },
	{ ".png", "image/png" },
	{ ".js", "text/javascript" },
	{ ".pdf", "application/pdf" },
	{ ".ps", "application/postscript" },
};

Response::Response(Request& request, int status, HeaderList headers)
	: request_(request), status_code_(status), headers_(headers)
{
	is_file_ = false;
	is_chunked_ = false;
}

Response::Response(const Response& response) : request_(response.request_) {
	this->operator=(response);
}

Response& Response::operator=(const Response& response) {
	request_ = response.request_;
	headers_ = response.headers_;
	status_code_ = response.status_code_;
	is_file_ = response.is_file_;
	is_chunked_ = response.is_chunked_;
	return *this;
}

Response::~Response() {
}


void Response::SetContent(std::string content) {
	is_file_ = false;
	is_chunked_ = false;
	int r = evbuffer_add(evhttp_request_get_output_buffer(request_.evrequest_),
		content.c_str(), content.length());
	if (r != 0)
		throw std::runtime_error("Failed to create add content to response buffer");
}

const Response::HeaderList& Response::GetHeaders() const {
	return headers_;
}

void Response::SetFilePath(std::string path) {
	is_file_ = true;
	int64_t size;
	int fd;
	if (!path.empty() && (fd = open_file(path, size)) > 0) {
		string ext = filesystem::path(path).extension().string();
		auto mime = content_type_table.find(ext);
		if (mime != content_type_table.end()) {
			evhttp_add_header(evhttp_request_get_output_headers(request_.evrequest_),
				"Content-type", mime->second.c_str());
		}
#ifdef NDEBUG
		//evbuffer_set_flags(response_buffer.get(), EVBUFFER_FLAG_DRAINS_TO_FD);
		int r = evbuffer_add_file(evhttp_request_get_output_buffer(request_.evrequest_), fd, 0, size);
		if (r != 0)
			throw std::runtime_error("Cannot add file content to buffer");
#else
		#warning this line will not work with release binary!
#endif
	}
	else
		throw std::runtime_error("Cannot open file");
}

void Response::SetChunkCallback(std::function<bool(std::string&)> func) { 
	chunk_callback_ = func;
	is_chunked_ = true;
}

struct chunk_req_state {
	evhttp_request* req;
	event* timer;
	std::function<bool(std::string&)> get_chunk;
	int i;
};

static void
schedule_trickle(struct chunk_req_state* state, int ms) {
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = ms * 30;
	evtimer_add(state->timer, &tv);
}

static void
http_chunked_trickle_cb(evutil_socket_t fd, short events, void* arg) {
	auto state = static_cast<chunk_req_state*>(arg);
	string chunk;
	if (state->get_chunk(chunk) && evhttp_request_get_connection(state->req)) {
		state->i++;
		struct evbuffer* evb = evbuffer_new();
		evbuffer_add(evb, chunk.c_str(), chunk.length());
		evhttp_send_reply_chunk(state->req, evb);
		evbuffer_free(evb);
		schedule_trickle(state, 1000);
	}
	else {
		evhttp_send_reply_end(state->req);
		event_free(state->timer);
		free(state);
	}
}

int Response::Send() {
	//--- add headers
	HeaderList default_headers = {
		{"Access-Control-Allow-Origin", "*"},
		{"Access-Control-Allow-Credentials", "true"},
		{"Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE"},
		{"Access-Control-Allow-Headers", "Origin, Accept, X-Requested-With, X-Auth-Token, "
			"Content-Type, Access-Control-Allow-Headers, "
			"Access-Control-Request-Method, Access-Control-Request-Headers"}
	};
	for (auto& h : default_headers)
		evhttp_add_header(evhttp_request_get_output_headers(request_.evrequest_),
			h.first.c_str(), h.second.c_str());

	for (auto& h : headers_)
		evhttp_add_header(evhttp_request_get_output_headers(request_.evrequest_),
			h.first.c_str(), h.second.c_str());

	if (!is_chunked_) 
		evhttp_send_reply(request_.evrequest_, status_code_, "ok", evhttp_request_get_output_buffer(request_.evrequest_));
	else {
		//https://gist.github.com/rgl/291085
		auto state = new chunk_req_state();
		state->req = request_.evrequest_;
		state->timer = evtimer_new(evhttp_connection_get_base(evhttp_request_get_connection(request_.evrequest_)),
			http_chunked_trickle_cb, state);
		state->get_chunk = chunk_callback_;
		evhttp_send_reply_start(request_.evrequest_, HTTP_OK, "OK");
		schedule_trickle(state, 0);
	}
	return 0;
}
