#include <easy_http/response.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <event2/event.h>

using namespace std;

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
	request_.SetContent(content);
}

const Response::HeaderList& Response::GetHeaders() const {
	return headers_;
}

void Response::SetFilePath(std::string path) {
	is_file_ = true;
	request_.SetFileContent(path);
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
