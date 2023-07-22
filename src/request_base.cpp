#include <easy_http/request_base.h>
#include <easy_http/response.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/event.h>
#include <sstream>
#include <algorithm>
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
};

struct CallBackContainer {
    bool is_complete = false;

    // events for server: request replying
    static void OnReplyComplete(struct evhttp_request* request, void* con_ptr);

    // events for client: request sending
    struct evhttp_connection* e_con_;
    RequestBase::ResponseHandler response_handler_;
    static void ResponseHandle(evhttp_request* request, void* con_ptr);
    static void ResponseChunkedHandle(evhttp_request* request, void* con_ptr);
    static void ResponseErrorHandle(enum evhttp_request_error err_code, void* con_ptr);
};
const std::map<string, string> RequestBase::content_types_ = {
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
RequestBase::RequestBase(evhttp_request* request)
    :type_(RequestType::RESPONSE), e_request_(request), response_handler_(nullptr), request_complete_(false)
{
    auto url = evhttp_request_get_uri(e_request_);
    auto uri = evhttp_uri_parse_with_flags(url, EVHTTP_URI_NONCONFORMANT);
    e_uri_ = shared_ptr<evhttp_uri>(uri, EventFree{});
    //e_uri_ = shared_ptr<evhttp_uri>(evhttp_request_get_evhttp_uri(request), EventFree{});
    cb_container_ = shared_ptr<CallBackContainer>(new CallBackContainer);
    //e_uri_is_owned_ = true;
    evhttp_request_set_on_complete_cb(e_request_, CallBackContainer::OnReplyComplete, cb_container_.get());

    queries_ = Queries();
    input_headers_ = Headers();
}

RequestBase::RequestBase(std::string url, ResponseHandler handler, bool is_chunked)
    :type_(RequestType::REQUEST), response_handler_(handler), is_chunked_(is_chunked), request_complete_(false)
{
    e_uri_ = shared_ptr<evhttp_uri>(evhttp_uri_parse(url.c_str()), EventFree{});
    cb_container_ = shared_ptr<CallBackContainer>(new CallBackContainer);
    cb_container_->response_handler_ = handler;
    e_request_ = evhttp_request_new(CallBackContainer::ResponseHandle, cb_container_.get());
    //e_uri_is_owned_ = true;
    if (is_chunked_)
        evhttp_request_set_chunked_cb(e_request_, CallBackContainer::ResponseChunkedHandle);
    evhttp_request_set_error_cb(e_request_, CallBackContainer::ResponseErrorHandle);

    queries_ = Queries();
}

RequestBase::RequestBase(RequestBase&& req) {
    *this = std::move(req);
}

RequestBase& RequestBase::operator=(RequestBase&& req) {
    if (this == &req)
        return *this;

    this->e_request_ = req.e_request_; req.e_request_ = nullptr;
    this->e_uri_ = req.e_uri_; req.e_uri_ = nullptr;
    this->cb_container_ = req.cb_container_; req.cb_container_ = nullptr;
    this->type_ = req.type_;
    this->response_handler_ = req.response_handler_;
    this->is_chunked_ = req.is_chunked_;
	if (req.chunk_callback_)
		this->chunk_callback_ = std::move(req.chunk_callback_);
    this->input_headers_ = std::move(req.input_headers_);
    this->queries_ = std::move(req.queries_);
    return *this;
}

RequestBase::RequestBase(const RequestBase& req) {
    *this = req;
}

RequestBase& RequestBase::operator=(const RequestBase& req) {
	if (this == &req)
		return *this;

    this->e_request_ = req.e_request_;
    this->e_uri_ = req.e_uri_;
    this->cb_container_ = req.cb_container_;
    this->type_ = req.type_;
    this->response_handler_ = req.response_handler_;
    this->is_chunked_ = req.is_chunked_;
    this->chunk_callback_ = req.chunk_callback_;
    this->input_headers_ = req.input_headers_;
    this->queries_ = req.queries_;
    return *this;
}

RequestBase::~RequestBase() {
    //if (evhttp_request_is_owned(e_request_))
    //    evhttp_request_free(e_request_);
}

#pragma endregion

#pragma region Public Methods

const std::string RequestBase::GetContent() const {
    string content;
    auto buffer = evhttp_request_get_input_buffer(e_request_);
    auto req_body_len = evbuffer_get_length(buffer);
    if (req_body_len > 0) {
        content.resize(req_body_len);
        evbuffer_copyout(buffer, (void*)content.data(), req_body_len);
    }
    return content;
}

RequestBase& RequestBase::SetContent(const std::string& content) {
    auto buffer = evhttp_request_get_output_buffer(e_request_);
    int r = evbuffer_add(buffer, content.c_str(), content.length());
    if (r != 0)
        throw std::runtime_error("Failed to create add content to response buffer");
    return *this;
}

RequestBase& RequestBase::SetContent(const std::vector<char>& content) {
    auto buffer = evhttp_request_get_output_buffer(e_request_);
    int r = evbuffer_add(buffer, content.data(), content.size());
    if (r != 0)
        throw std::runtime_error("Failed to create add content to response buffer");
    return *this;
}

RequestBase& RequestBase::SetFileContent(std::string file_path) {
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

const RequestBase::ParamList RequestBase::Queries() const {
    ParamList list;
#if 1
    auto qury_char = evhttp_uri_get_query(e_uri_.get());
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
    evhttp_parse_query(evhttp_request_get_uri(RequestBase), &queries);
    auto q_keyval = queries.tqh_first;
    while (q_keyval) {
        RequestBase_info.uri_query[q_keyval->key] = q_keyval->value;
        q_keyval = q_keyval->next.tqe_next;
    }
#endif
    return list;
}

const RequestBase::ParamList RequestBase::Headers() const {
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

RequestBase& RequestBase::PushHeader(const std::string& key, const std::string& value) {
    int r = evhttp_add_header(evhttp_request_get_output_headers(e_request_),
        key.c_str(), value.c_str());
    if (r != 0)
        throw runtime_error("Cannot set header " + key);
    return *this;
}

RequestBase& RequestBase::PushHeader(const RequestBase::ParamList& headers) {
    for (auto& h : headers)
		PushHeader(h.first, h.second);
    return *this;
}


const RequestBase::RequestMethod RequestBase::Method() const {
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

const std::string RequestBase::FullUrl() const {
    if (evhttp_request_get_uri(e_request_))
        return evhttp_request_get_uri(e_request_);
    char buf[URL_MAX];
    if (evhttp_uri_join(e_uri_.get(), buf, URL_MAX))
        return string(buf);
    return "";
}

const std::string RequestBase::Scheme() const {
    return evhttp_uri_get_scheme(e_uri_.get()) ? string(evhttp_uri_get_scheme(e_uri_.get())) : "";
}

const std::string RequestBase::User() const {
    return evhttp_uri_get_userinfo(e_uri_.get()) ? string(evhttp_uri_get_userinfo(e_uri_.get())) : "";
}

const std::string RequestBase::Host() const {
    return evhttp_uri_get_host(e_uri_.get()) ? string(evhttp_uri_get_host(e_uri_.get())) : "";
}

const int RequestBase::Port() const {
    return evhttp_uri_get_port(e_uri_.get()) > 0 ? evhttp_uri_get_port(e_uri_.get()) : 80;
}

const std::string RequestBase::Path() const {
    auto s = evhttp_uridecode(evhttp_uri_get_path(e_uri_.get()), 0, NULL);
    auto path = evhttp_uri_get_path(e_uri_.get()) ? string(s) : "";
    free(s);
    return path;
}

const int RequestBase::ResponseCode() const {
    return evhttp_request_get_response_code(e_request_);
}

RequestBase& RequestBase::SetChunkCallback(std::function<bool(std::string&)> func) {
    chunk_callback_ = func;
    is_chunked_ = true;
    return *this;
}

const int RequestBase::ConnectionPort() const {
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

const std::string RequestBase::ConnectionAddress() const {
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

bool RequestBase::UrlIs(std::string pattern, std::vector<std::string>& vars) const {
    std::smatch base_match;
    string path = Path();
    if (std::regex_match(path, base_match, std::regex(pattern))) {
        for (int i = 1; i < base_match.size(); i++)
            vars.push_back(base_match[i].str());
        return true;
    }
    return false;
}

bool RequestBase::UrlIs(std::string pattern) const {
    std::smatch base_match;
	string path = Path();
	return std::regex_match(path, base_match, std::regex(pattern));
}

void RequestBase::Cancel() {
    return evhttp_cancel_request(e_request_);
}

int RequestBase::Wait() {
    if (evhttp_request_get_connection(e_request_)) {
        auto base = evhttp_connection_get_base(evhttp_request_get_connection(e_request_));
        return event_base_dispatch(base);
    }
    else
        return 0;
}

bool RequestBase::IsComplete() {
	return cb_container_->is_complete;
}

#pragma endregion

#pragma region Protected Methods
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

void RequestBase::Reply(int status_code) {
    if (type_ == RequestType::REQUEST)
        throw runtime_error("Invalid RequestBase type! use Send function.");

    //--- add headers
    ParamList default_headers = {
        {"access-control-allow-origin", "*"},
        {"access-control-allow-credentials", "true"},
        {"access-control-allow-methods", "GET, POST, OPTIONS, PUT, PATCH, DELETE"},
        {"access-control-allow-headers", "Origin, Accept, X-Requested-With, X-Auth-Token, "
            "Content-Type, Access-Control-Allow-Headers, "
            "Access-Control-Request-Method, Access-Control-Request-Headers"}
    };
    PushHeader(default_headers);

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

void RequestBase::SendAsync(struct evhttp_connection* e_con, RequestBase::RequestMethod method) {
    if (type_ == RequestType::RESPONSE)
        throw runtime_error("Invalid RequestBase type! use Reply function.");

    evhttp_cmd_type m;
#undef DELETE
    switch (method) {
    case RequestBase::RequestMethod::GET:       m = EVHTTP_REQ_GET; break;
    case RequestBase::RequestMethod::POST:      m = EVHTTP_REQ_POST; break;
    case RequestBase::RequestMethod::HEAD:      m = EVHTTP_REQ_HEAD; break;
    case RequestBase::RequestMethod::PUT:       m = EVHTTP_REQ_PUT; break;
    case RequestBase::RequestMethod::DELETE:    m = EVHTTP_REQ_DELETE; break;
    case RequestBase::RequestMethod::OPTIONS:   m = EVHTTP_REQ_OPTIONS; break;
    case RequestBase::RequestMethod::TRACE:     m = EVHTTP_REQ_TRACE; break;
    case RequestBase::RequestMethod::CONNECT:   m = EVHTTP_REQ_CONNECT; break;
    case RequestBase::RequestMethod::PATCH:     m = EVHTTP_REQ_PATCH; break;
    }
#pragma pop_macro("DELETE")
    cb_container_->e_con_ = e_con;
    if (evhttp_make_request(e_con, e_request_, m, FullUrl().c_str()) == -1)
        throw std::exception("Request failed!");
}

void RequestBase::Send(struct evhttp_connection* e_con, RequestBase::RequestMethod method) {
    SendAsync(e_con, method);
    int r = event_base_dispatch(evhttp_connection_get_base(e_con));
    e_con = NULL;
}
#pragma endregion

#pragma region Private Methods

std::string RequestBase::ToLower(const std::string& str) const {
    string r = str;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return r;
}

void CallBackContainer::OnReplyComplete(struct evhttp_request* request, void* con_ptr) {
    auto con = static_cast<CallBackContainer*>(con_ptr);
    con->is_complete = true;
}

void CallBackContainer::ResponseHandle(evhttp_request* request, void* con_ptr) {
    auto con = static_cast<CallBackContainer*>(con_ptr);
    con->response_handler_(Response(request));
    event_base_loopexit(evhttp_connection_get_base(con->e_con_), NULL);
    con->is_complete = true;
}

void CallBackContainer::ResponseChunkedHandle(evhttp_request* request, void* con_ptr) {
    auto con = static_cast<CallBackContainer*>(con_ptr);
    con->response_handler_(Response(request));
}

void CallBackContainer::ResponseErrorHandle(enum evhttp_request_error err_code, void* con_ptr) {
    switch (err_code)
    {
    case EVREQ_HTTP_TIMEOUT:
        throw runtime_error("RequestBase timeout");
        break;
    case EVREQ_HTTP_EOF:
        throw runtime_error("RequestBase EOF");
        break;
    case EVREQ_HTTP_INVALID_HEADER:
        throw runtime_error("RequestBase invalid header");
        break;
    case EVREQ_HTTP_BUFFER_ERROR:
        throw runtime_error("RequestBase buffer error");
        break;
    case EVREQ_HTTP_REQUEST_CANCEL:
        throw runtime_error("RequestBase cancel");
        break;
    case EVREQ_HTTP_DATA_TOO_LONG:
        throw runtime_error("RequestBase data too long");
        break;
    default:
        break;
    }
}


bool RequestBase::IsFilled(const std::map<std::string, std::string>& list, std::string key) const {
    return (list.find(key) != list.end()) && !list.at(key).empty();
}

//-----
template<>
std::string RequestBase::GetVal(std::string str_value) const {
    return str_value;
}

template<>
int RequestBase::GetVal(std::string str_value) const {
    return stoi(str_value);
}

template<>
double RequestBase::GetVal(std::string str_value) const {
    return stod(str_value);
}

template<>
bool RequestBase::GetVal(std::string str_value) const {
    return stoi(str_value);
}

// time format should be %Y-%m-%d or %Y-%m-%d-%H-%M-%S
template<>
time_t RequestBase::GetVal(std::string str_value) const {
    if (std::count(str_value.begin(), str_value.end(), '-') == 2)
        str_value += "-00-00-00";
    return str2time(str_value, "%Y-%m-%d-%H-%M-%S");
}

template<>
vector<int> RequestBase::GetVal(std::string str_value) const {
    vector<int> result;
    stringstream ss(str_value);
    while (ss.good()) {
        string num;
        getline(ss, num, ',');
        result.push_back(stoi(num));
    }
    return result;
}

#pragma endregion
