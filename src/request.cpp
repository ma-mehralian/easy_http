#include <easy_http/request.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/event.h>
#include <regex>
#include <algorithm>
#include <sstream>
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

struct EventFree {
    void operator()(evhttp_uri* uri) const { evhttp_uri_free(uri); }
    void operator()(const evhttp_uri* uri) const { /*evhttp_uri_free(uri);*/ }
    void operator()(evkeyvalq* headers) const { evhttp_clear_headers(headers); }
};
template<typename T>
using event_ptr = std::unique_ptr<T, EventFree>;

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

Request::Request(evhttp_request* request) : evrequest_(request) {
#pragma push_macro("DELETE")
#undef DELETE
    // read reaquest type
    switch (evhttp_request_get_command(request)) {
    case EVHTTP_REQ_GET:        method_ = RequestMethod::GET; break;
    case EVHTTP_REQ_POST:       method_ = RequestMethod::POST; break;
    case EVHTTP_REQ_HEAD:       method_ = RequestMethod::HEAD; break;
    case EVHTTP_REQ_PUT:        method_ = RequestMethod::PUT; break;
    case EVHTTP_REQ_DELETE:     method_ = RequestMethod::DELETE; break;
    case EVHTTP_REQ_OPTIONS:    method_ = RequestMethod::OPTIONS; break;
    case EVHTTP_REQ_TRACE:      method_ = RequestMethod::TRACE; break;
    case EVHTTP_REQ_CONNECT:    method_ = RequestMethod::CONNECT; break;
    case EVHTTP_REQ_PATCH:      method_ = RequestMethod::PATCH; break;
    }
#pragma pop_macro("DELETE")
    
    // read connection info
	auto conn = evhttp_request_get_connection(request);
	if (conn) {
#if 1
		const char* address;
		ev_uint16_t port;
		evhttp_connection_get_peer(conn, &address, &port);
		client_ip_ = string(address);
		client_port_ = port;
#else //BOTH METHODS MAKE A SIMILAR RESULTS!
		auto s = evhttp_connection_get_addr(conn);
		if (s) {
			char IP[128];
			switch (s->sa_family)
			{
			case AF_INET:  // IPv4:
			{
				const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(s);
				evutil_inet_ntop(AF_INET, &(sin->sin_addr), IP, sizeof(IP));
				client_ip_ = string(IP);
				client_port_ = ntohs(sin->sin_port);
				break;
			}
			case AF_INET6:  // IPv6
			{
				const sockaddr_in6* sin = reinterpret_cast<const sockaddr_in6*>(s);
				evutil_inet_ntop(AF_INET6, &(sin->sin6_addr), IP, sizeof(IP));
				client_ip_ = string(IP);
				client_port_ = ntohs(sin->sin6_port);
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

    ParsUri(evhttp_request_get_uri(request));

    // read request headers
    //event_ptr<evkeyvalq> headers(evhttp_request_get_input_headers(request));
    auto headers = evhttp_request_get_input_headers(request);
    auto h_keyval = headers->tqh_first;
    while (h_keyval){
        input_headers_[h_keyval->key] = h_keyval->value;
        h_keyval = h_keyval->next.tqe_next;
    }
}

Request::Request(evhttp_request* request, Request::RequestMethod method, std::string url)
	:evrequest_(request), method_(method)
{
    ParsUri(url);
}

Request::~Request() {
    if (evhttp_request_is_owned(evrequest_))
        evhttp_request_free(evrequest_);
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

void Request::Reply(int status_code) {
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
        evhttp_add_header(evhttp_request_get_output_headers(evrequest_),
            h.first.c_str(), h.second.c_str());

    for (auto& h : output_headers_)
        evhttp_add_header(evhttp_request_get_output_headers(evrequest_),
            h.first.c_str(), h.second.c_str());

    if (!is_chunked_)
        evhttp_send_reply(evrequest_, status_code, "ok", evhttp_request_get_output_buffer(evrequest_));
    else {
        //https://gist.github.com/rgl/291085
        auto state = new chunk_req_state();
        state->req = evrequest_;
        state->timer = evtimer_new(evhttp_connection_get_base(evhttp_request_get_connection(evrequest_)),
            http_chunked_trickle_cb, state);
        state->get_chunk = chunk_callback_;
        evhttp_send_reply_start(evrequest_, status_code, "OK");
        schedule_trickle(state, 0);
    }
}

Request& Request::SetChunkCallback(std::function<bool(std::string&)> func) {
    chunk_callback_ = func;
    is_chunked_ = true;
    return *this;
}

int Request::Wait() {
    auto base = evhttp_connection_get_base(evhttp_request_get_connection(evrequest_));
    return event_base_dispatch(base);
}

const std::string Request::GetContent() const {
    string content;
    auto buffer = evhttp_request_get_input_buffer(evrequest_);
    auto req_body_len = evbuffer_get_length(buffer);
    if (req_body_len > 0) {
        content.resize(req_body_len);
        evbuffer_copyout(buffer, (void*)content.data(), req_body_len);
    }
    return content;
}

Request& Request::SetContent(const std::string& content) {
    auto buffer = evhttp_request_get_output_buffer(evrequest_);
    int r = evbuffer_add(buffer, content.c_str(), content.length());
    if (r != 0)
        throw std::runtime_error("Failed to create add content to response buffer");
    return *this;
}

Request& Request::SetContent(const std::vector<char>& content) {
    auto buffer = evhttp_request_get_output_buffer(evrequest_);
    int r = evbuffer_add(buffer, content.data(), content.size());
    if (r != 0)
        throw std::runtime_error("Failed to create add content to response buffer");
    return *this;
}

Request& Request::SetFileContent(std::string file_path) {
    int64_t size;
    int fd;
    if (!file_path.empty() && (fd = open_file(file_path, size)) > 0) {
        string ext = FS::path(file_path).extension().string();
        auto mime = content_type_table.find(ext);
        if (mime != content_type_table.end())
            PushHeader("Content-type", mime->second);
#ifdef NDEBUG
        //evbuffer_set_flags(response_buffer.get(), EVBUFFER_FLAG_DRAINS_TO_FD);
        int r = evbuffer_add_file(evhttp_request_get_output_buffer(evrequest_), fd, 0, size);
        if (r != 0)
            throw std::runtime_error("Cannot add file content to buffer");
#else
        //#warning this line will not work with release binary!
#endif
    }
    else
        throw std::runtime_error("Cannot open file");
    return *this;
}


Request& Request::PushHeader(std::string key, std::string value) { 
    output_headers_[key] = value;
    return *this;
}


Request& Request::SetHeaders(const HeaderList& headers) { 
    output_headers_ = headers; 
    return *this;
}

#ifdef USE_JSON
nlohmann::json Request::Json() const {
    try {
        return nlohmann::json::parse(body_);
    }
    catch (const exception e) {
        printf("Invalid json: %s", e.what());
        throw e;
    }
}
#endif // USE_JSON

bool Request::UrlIs(std::string pattern, std::vector<std::string>& vars) const {
	std::smatch base_match;
    if (std::regex_match(uri_.path, base_match, std::regex(pattern))) {
        for (int i = 1; i < base_match.size(); i++)
            vars.push_back(base_match[i].str());
        return true;
    }
    return false;
}

bool Request::UrlIs(std::string pattern) const {
	std::smatch base_match;
    return std::regex_match(uri_.path, base_match, std::regex(pattern));
}

void Request::ParsUri(std::string url) {
     uri_.full_path = url;
    //event_ptr<const evhttp_uri> e_uri(evhttp_request_get_evhttp_uri(request));
    event_ptr<evhttp_uri> e_uri(evhttp_uri_parse(url.c_str()));
    if (e_uri != nullptr) {
        //uri_.full_path = evhttp_request_get_uri(request) ? string(evhttp_uridecode(evhttp_request_get_uri(request), 0, NULL)) : "";
        uri_.path = evhttp_uri_get_path(e_uri.get()) ? string(evhttp_uridecode(evhttp_uri_get_path(e_uri.get()), 0, NULL)) : "";
        uri_.scheme = evhttp_uri_get_scheme(e_uri.get()) ? string(evhttp_uri_get_scheme(e_uri.get())) : "";
        uri_.userinfo = evhttp_uri_get_userinfo(e_uri.get()) ? string(evhttp_uri_get_userinfo(e_uri.get())) : "";
        uri_.port = evhttp_uri_get_port(e_uri.get());
        uri_.fragment = evhttp_uri_get_fragment(e_uri.get()) ? string(evhttp_uri_get_fragment(e_uri.get())) : "";
        uri_.host = evhttp_uri_get_host(e_uri.get()) ? string(evhttp_uri_get_host(e_uri.get())) : "";

        //--- read request query string
#if 1
        auto qury_char = evhttp_uri_get_query(e_uri.get());
        if (qury_char) {
            string query_str = qury_char;
            std::regex pattern("([^&=]+)=([^&=]*)");
            auto words_begin = std::sregex_iterator(query_str.begin(), query_str.end(), pattern);
            auto words_end = std::sregex_iterator();
            for (std::sregex_iterator i = words_begin; i != words_end; i++) {
                std::string key = evhttp_uridecode((*i)[1].str().c_str(), 0, NULL);
                std::string value = evhttp_uridecode((*i)[2].str().c_str(), 0, NULL);
                //--- array params
                if (key.size() > 2 && key.substr(key.size() - 2) == "[]") {
                    key = key.substr(0, key.size() - 2);
                    if (uri_.query.find(key) != uri_.query.end())
                        uri_.query[key] += "," + value;
                    else
                        uri_.query[key] = value;
                }
                else
                    uri_.query[key] = value;
            }
        }
#else
        // DISABLED DUE TO UNKNOW ERROR
        evkeyvalq queries;
        event_ptr<evkeyvalq> queries_ptr(&queries);
        evhttp_parse_query(evhttp_request_get_uri(request), &queries);
        auto q_keyval = queries.tqh_first;
        while (q_keyval) {
            request_info.uri_query[q_keyval->key] = q_keyval->value;
            q_keyval = q_keyval->next.tqe_next;
        }
#endif
    }
}

//--- utils functions
bool Request::IsFilled(const std::map<std::string, std::string>& list, std::string key) const {
    return (list.find(key) != list.end()) && !list.at(key).empty();
}

//-----
template<>
std::string Request::GetVal(std::string str_value) const {
    return str_value;
}

template<>
int Request::GetVal(std::string str_value) const {
    return stoi(str_value);
}

template<>
double Request::GetVal(std::string str_value) const {
    return stod(str_value);
}

template<>
bool Request::GetVal(std::string str_value) const {
    return stoi(str_value);
}

// time format should be %Y-%m-%d or %Y-%m-%d-%H-%M-%S
template<>
time_t Request::GetVal(std::string str_value) const {
	if (std::count(str_value.begin(), str_value.end(), '-') == 2)
		str_value += "-00-00-00";
	return str2time(str_value, "%Y-%m-%d-%H-%M-%S");
}

template<>
vector<int> Request::GetVal(std::string str_value) const {
	vector<int> result;
	stringstream ss(str_value);
	while (ss.good()) {
		string num;
		getline(ss, num, ',');
		result.push_back(stoi(num));
	}
	return result;
}
    