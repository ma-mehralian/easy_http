#include <easy_http/ev_request.h>
#include <easy_http/response.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/event.h>
#include "utils.h"

#if __has_include(<filesystem>)
#include <filesystem>
namespace FS = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace FS = std::experimental::filesystem;
#else
#error "no filesystem support!"
#endif

using namespace std;

#define URL_MAX 4096

struct EventFree {
    void operator()(evhttp_uri* uri) const { evhttp_uri_free(uri); }
    void operator()(const evhttp_uri* uri) const { /*evhttp_uri_free(uri);*/ }
    void operator()(evkeyvalq* headers) const { evhttp_clear_headers(headers); }
};
template<typename T>
using event_ptr = std::unique_ptr<T, EventFree>;

const std::map<string, string> EvRequest::content_types_ = {
    { "",       "text/html" },
    { ".html",  "text/html" },
    { ".htm",   "text/htm" },
    { ".txt",   "text/plain" },
    { ".css",   "text/css" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".png",   "image/png" },
    { ".js",    "text/javascript" },
    { ".pdf",   "application/pdf" },
    { ".ps",    "application/postscript" },
};

#pragma region (De)Constructors
EvRequest::EvRequest(EvRequest&& req) {
    *this = std::move(req);
}

//EvRequest& EvRequest::operator=(const EvRequest& req) {
//	if (this == &req)
//		return *this;
//
//    this->type_ = req.type_;
//    this->method_ = req.method_;
//    this->e_request_ = req.e_request_;
//    this->e_conn_ = req.e_conn_;
//    this->response_handler_ = req.response_handler_;
//    this->client_ip_ = req.client_ip_;
//    this->client_port_ = req.client_port_;
//    this->uri_ = req.uri_;
//    this->input_headers_ = req.input_headers_;
//    this->is_chunked_ = req.is_chunked_;
//    this->chunk_callback_ = req.chunk_callback_;
//    this->this_container_ = new EvRequest*;
//    *this->this_container_ = this;
//    return *this;
//}

EvRequest& EvRequest::operator=(EvRequest&& req) {
    if (this == &req)
        return *this;

    this->type_ = req.type_;
    this->e_request_ = req.e_request_;
    this->response_handler_ = req.response_handler_;
    this->is_chunked_ = req.is_chunked_;
    this->chunk_callback_ = std::move(req.chunk_callback_);
    req.e_request_ = nullptr;
    return *this;
}

EvRequest::EvRequest(evhttp_request* request)
    :type_(RequestType::RESPONSE), e_request_(request), response_handler_(nullptr), request_complete_(false)
{
    e_uri_ = evhttp_uri_parse(evhttp_request_get_uri(e_request_));
    evhttp_request_set_on_complete_cb(e_request_, EvRequest::OnReplyComplete, this);
}

EvRequest::EvRequest(std::string url, Handler handler, bool is_chunked)
    :type_(RequestType::REQUEST), response_handler_(handler), is_chunked_(is_chunked), request_complete_(false)
{
    e_request_ = evhttp_request_new(EvRequest::ResponseHandler, this);
    e_uri_ = evhttp_uri_parse(url.c_str());
    if (is_chunked_)
        evhttp_request_set_chunked_cb(e_request_, EvRequest::ResponseChunkedHandler);
    evhttp_request_set_error_cb(e_request_, EvRequest::ResponseErrorHandler);
}

EvRequest::~EvRequest() {
    if (evhttp_request_is_owned(e_request_))
        evhttp_request_free(e_request_);
}

#pragma endregion

#pragma region Public Methods

const std::string EvRequest::GetContent() const {
    string content;
    auto buffer = evhttp_request_get_input_buffer(e_request_);
    auto req_body_len = evbuffer_get_length(buffer);
    if (req_body_len > 0) {
        content.resize(req_body_len);
        evbuffer_copyout(buffer, (void*)content.data(), req_body_len);
    }
    return content;
}

EvRequest& EvRequest::SetContent(const std::string& content) {
    auto buffer = evhttp_request_get_output_buffer(e_request_);
    int r = evbuffer_add(buffer, content.c_str(), content.length());
    if (r != 0)
        throw std::runtime_error("Failed to create add content to response buffer");
    return *this;
}

EvRequest& EvRequest::SetContent(const std::vector<char>& content) {
    auto buffer = evhttp_request_get_output_buffer(e_request_);
    int r = evbuffer_add(buffer, content.data(), content.size());
    if (r != 0)
        throw std::runtime_error("Failed to create add content to response buffer");
    return *this;
}

EvRequest& EvRequest::SetFileContent(std::string file_path) {
    int64_t size;
    int fd;
    if (!file_path.empty() && (fd = open_file(file_path, size)) > 0) {
        string ext = FS::path(file_path).extension().string();
        auto mime = content_types_.find(ext);
        if (mime != content_types_.end())
            PushHeader("content-type", mime->second);
//#ifdef NDEBUG
        //evbuffer_set_flags(response_buffer.get(), EVBUFFER_FLAG_DRAINS_TO_FD);
        int r = evbuffer_add_file(evhttp_request_get_output_buffer(e_request_), fd, 0, size);
        if (r != 0)
            throw std::runtime_error("Cannot add file content to buffer");
//#else
        //#warning this line will not work with release binary!
//#endif
    }
    else
        throw std::runtime_error("Cannot open file");
    return *this;
}

EvRequest& EvRequest::PushHeader(const std::string& key, const std::string& value) {
	int r = evhttp_add_header(evhttp_request_get_output_headers(e_request_),
        ToLower(key).c_str(), value.c_str());
    if (r != 0)
        throw runtime_error("Cannot set header " + key);
    return *this;
}

const EvRequest::RequestMethod EvRequest::Method() const {
#pragma push_macro("DELETE")
#undef DELETE
    // read reaquest type
    switch (evhttp_request_get_command(e_request_)) {
    case EVHTTP_REQ_GET:        return RequestMethod::GET; break;
    case EVHTTP_REQ_POST:       return RequestMethod::POST; break;
    case EVHTTP_REQ_HEAD:       return RequestMethod::HEAD; break;
    case EVHTTP_REQ_PUT:        return RequestMethod::PUT; break;
    case EVHTTP_REQ_DELETE:     return RequestMethod::DELETE; break;
    case EVHTTP_REQ_OPTIONS:    return RequestMethod::OPTIONS; break;
    case EVHTTP_REQ_TRACE:      return RequestMethod::TRACE; break;
    case EVHTTP_REQ_CONNECT:    return RequestMethod::CONNECT; break;
    case EVHTTP_REQ_PATCH:      return RequestMethod::PATCH; break;
    }
#pragma pop_macro("DELETE")
}

const std::string EvRequest::FullUrl() const { 
    if (evhttp_request_get_uri(e_request_))
        return evhttp_request_get_uri(e_request_);
    char buf[URL_MAX];
    if (evhttp_uri_join(e_uri_, buf, URL_MAX))
        return string(buf);
	return "";
}

const std::string EvRequest::Scheme() const { 
    return evhttp_uri_get_scheme(e_uri_) ? string(evhttp_uri_get_scheme(e_uri_)) : "";
    event_ptr<const evhttp_uri> e_uri(evhttp_request_get_evhttp_uri(e_request_));
    return evhttp_uri_get_scheme(e_uri.get()) ? string(evhttp_uri_get_scheme(e_uri.get())) : "";
}

const std::string EvRequest::User() const { 
    return evhttp_uri_get_userinfo(e_uri_) ? string(evhttp_uri_get_userinfo(e_uri_)) : "";
    event_ptr<const evhttp_uri> e_uri(evhttp_request_get_evhttp_uri(e_request_));
    return evhttp_uri_get_userinfo(e_uri.get()) ? string(evhttp_uri_get_userinfo(e_uri.get())) : "";
}

const std::string EvRequest::Host() const { 
    return evhttp_uri_get_host(e_uri_) ? string(evhttp_uri_get_host(e_uri_)) : "";
    event_ptr<const evhttp_uri> e_uri(evhttp_request_get_evhttp_uri(e_request_));
    return evhttp_uri_get_host(e_uri.get()) ? string(evhttp_uri_get_host(e_uri.get())) : "";
}

const int EvRequest::Port() const { 
    return evhttp_uri_get_port(e_uri_) > 0 ? evhttp_uri_get_port(e_uri_) : 80;
    event_ptr<const evhttp_uri> e_uri(evhttp_request_get_evhttp_uri(e_request_));
    return evhttp_uri_get_port(e_uri.get()) > 0 ? evhttp_uri_get_port(e_uri.get()) : 80;
}

const std::string EvRequest::Path() const {
	event_ptr<const evhttp_uri> e_uri(evhttp_request_get_evhttp_uri(e_request_));
	auto s = evhttp_uridecode(evhttp_uri_get_path(e_uri.get()), 0, NULL);
	auto path = evhttp_uri_get_path(e_uri.get()) ? string(s) : "";
	free(s);
	return path;
}

const EvRequest::ParamList EvRequest::Queries() const {
    ParamList list;
#if 1
    auto qury_char = evhttp_uri_get_query(e_uri_);
    if (qury_char) {
        string query_str = qury_char;
        std::regex pattern("([^&=]+)=([^&=]*)");
        auto words_begin = std::sregex_iterator(query_str.begin(), query_str.end(), pattern);
        auto words_end = std::sregex_iterator();
        for (std::sregex_iterator i = words_begin; i != words_end; i++) {
            char* k = evhttp_uridecode((*i)[1].str().c_str(), 1, NULL);
            char* v = evhttp_uridecode((*i)[2].str().c_str(), 1, NULL);
            std::string key(k);
            std::string value(v);
            free(k);
            free(v);
            //--- array params
            if (key.size() > 2 && key.substr(key.size() - 2) == "[]") {
                key = key.substr(0, key.size() - 2);
                if (list.find(key) != list.end())
                    list[key] += "," + value; // array queries
                else
                    list[key] = value;
            }
            else
                list[key] = value;
        }
    }
#else
    // DISABLED DUE TO UNKNOW ERROR
    evkeyvalq queries;
    event_ptr<evkeyvalq> queries_ptr(&queries);
    evhttp_parse_query(evhttp_request_get_uri(EvRequest), &queries);
    auto q_keyval = queries.tqh_first;
    while (q_keyval) {
        EvRequest_info.uri_query[q_keyval->key] = q_keyval->value;
        q_keyval = q_keyval->next.tqe_next;
    }
#endif
    return list;
}

const EvRequest::ParamList EvRequest::Headers() const {
    ParamList list;
    //event_ptr<evkeyvalq> headers(evhttp_request_get_input_headers(e_request_));
    auto headers = evhttp_request_get_input_headers(e_request_);
    auto h_keyval = headers->tqh_first;
    while (h_keyval) {
        list[ToLower(h_keyval->key)] = h_keyval->value;
        h_keyval = h_keyval->next.tqe_next;
    }
    return list;
}

const int EvRequest::ResponseCode() const { 
    return evhttp_request_get_response_code(e_request_);
}

EvRequest& EvRequest::SetChunkCallback(std::function<bool(std::string&)> func) {
    chunk_callback_ = func;
    is_chunked_ = true;
    return *this;
}

int EvRequest::Wait() {
    if (evhttp_request_get_connection(e_request_)) {
		auto base = evhttp_connection_get_base(evhttp_request_get_connection(e_request_));
		return event_base_dispatch(base);
    }
    else
        return 0;
}

const int EvRequest::ConnectionPort() const {
	auto e_conn = evhttp_request_get_connection(e_request_);
    string adrs = "";
	uint16_t port = -1;
	if (e_conn) {
#if 1
		const char* address;
		evhttp_connection_get_peer(e_conn, &address, &port);
		adrs = string(address);

#else //BOTH METHODS MAKE A SIMILAR RESULTS!
		auto s = evhttp_connection_get_addr(e_conn);
		if (s) {
			char IP[128];
			switch (s->sa_family)
			{
			case AF_INET:  // IPv4:
			{
				const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(s);
				evutil_inet_ntop(AF_INET, &(sin->sin_addr), IP, sizeof(IP));
				adrs = string(IP);
				port = ntohs(sin->sin_port);
				break;
			}
			case AF_INET6:  // IPv6
			{
				const sockaddr_in6* sin = reinterpret_cast<const sockaddr_in6*>(s);
				evutil_inet_ntop(AF_INET6, &(sin->sin6_addr), IP, sizeof(IP));
				adrs = string(IP);
				port = ntohs(sin->sin6_port);
				break;
			}

			default:
			{
				printf("%s: Unknown socket address family: %d", __FUNCTION__, s->sa_family);
				break;
			}
			}
		}
#endif
	}
	return port;
}

const std::string EvRequest::ConnectionAddress() const {
	auto e_conn = evhttp_request_get_connection(e_request_);
    string adrs = "";
    uint16_t port = -1;
	if (e_conn) {
		const char* address;
		evhttp_connection_get_peer(e_conn, &address, &port);
        adrs = string(address);
	}
    return adrs;

}

void EvRequest::Cancel() {
    return evhttp_cancel_request(e_request_);
}

#pragma endregion

#pragma region Protected Methods
std::string EvRequest::ToLower(const std::string& str) const {
    string r = str;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return r;
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

void EvRequest::Reply(int status_code) {
    if (type_ == RequestType::REQUEST)
        throw runtime_error("Invalid EvRequest type! use Send function.");

    if (!is_chunked_)
        evhttp_send_reply(e_request_, status_code, "ok", evhttp_request_get_output_buffer(e_request_));
    else {
        //https://gist.github.com/rgl/291085
        auto state = new chunk_req_state();
        state->req = e_request_;
        state->timer = evtimer_new(evhttp_connection_get_base(evhttp_request_get_connection(e_request_)),
            http_chunked_trickle_cb, state);
        state->get_chunk = chunk_callback_;
        evhttp_send_reply_start(e_request_, status_code, "OK");
        schedule_trickle(state, 0);
    }
}

void EvRequest::SendAsync(struct evhttp_connection* e_con, EvRequest::RequestMethod method) {
    if (type_ == RequestType::RESPONSE)
        throw runtime_error("Invalid EvRequest type! use Reply function.");

    evhttp_cmd_type m;
#undef DELETE
    switch (method) {
    case EvRequest::RequestMethod::GET:       m = EVHTTP_REQ_GET; break;
    case EvRequest::RequestMethod::POST:      m = EVHTTP_REQ_POST; break;
    case EvRequest::RequestMethod::HEAD:      m = EVHTTP_REQ_HEAD; break;
    case EvRequest::RequestMethod::PUT:       m = EVHTTP_REQ_PUT; break;
    case EvRequest::RequestMethod::DELETE:    m = EVHTTP_REQ_DELETE; break;
    case EvRequest::RequestMethod::OPTIONS:   m = EVHTTP_REQ_OPTIONS; break;
    case EvRequest::RequestMethod::TRACE:     m = EVHTTP_REQ_TRACE; break;
    case EvRequest::RequestMethod::CONNECT:   m = EVHTTP_REQ_CONNECT; break;
    case EvRequest::RequestMethod::PATCH:     m = EVHTTP_REQ_PATCH; break;
    }
#pragma pop_macro("DELETE")
    if (evhttp_make_request(e_con, e_request_, m, FullUrl().c_str()) == -1)
        throw std::exception("Request failed!");
}

void EvRequest::Send(struct evhttp_connection* e_con, EvRequest::RequestMethod method) {
    SendAsync(e_con, method);
    int r = event_base_dispatch(evhttp_connection_get_base(e_con));
    e_con = NULL;
}
#pragma endregion

#pragma region Private Methods

void EvRequest::OnReplyComplete(struct evhttp_request* request, void* arg) {
    auto req = static_cast<EvRequest*>(arg);
    req->request_complete_ = true;
}

void EvRequest::ResponseHandler(evhttp_request* request, void* request_ptr) {
    auto req = static_cast<EvRequest*>(request_ptr);
    req->response_handler_(make_unique<EvRequest>(request));
    auto con = evhttp_request_get_connection(req->e_request_);
    auto b = evhttp_connection_get_base(con);
    event_base_loopexit(b, NULL);
    req->request_complete_ = true;
}

void EvRequest::ResponseChunkedHandler(evhttp_request* request, void* request_ptr) {
    auto req = *static_cast<EvRequest**>(request_ptr);
    req->response_handler_(make_unique<EvRequest>(request));
}

void EvRequest::ResponseErrorHandler(enum evhttp_request_error err_code, void* request_ptr) {
    switch (err_code)
    {
    case EVREQ_HTTP_TIMEOUT:
        throw runtime_error("EvRequest timeout");
        break;
    case EVREQ_HTTP_EOF:
        throw runtime_error("EvRequest EOF");
        break;
    case EVREQ_HTTP_INVALID_HEADER:
        throw runtime_error("EvRequest invalid header");
        break;
    case EVREQ_HTTP_BUFFER_ERROR:
        throw runtime_error("EvRequest buffer error");
        break;
    case EVREQ_HTTP_REQUEST_CANCEL:
        throw runtime_error("EvRequest cancel");
        break;
    case EVREQ_HTTP_DATA_TOO_LONG:
        throw runtime_error("EvRequest data too long");
        break;
    default:
        break;
    }
}

#pragma endregion
