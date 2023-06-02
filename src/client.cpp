#include <easy_http/client.h>
#include <signal.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/dns.h>

#ifdef EVENT__HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef EVENT__HAVE_AFUNIX_H
#include <afunix.h>
#endif

#define URL_MAX 4096

using namespace std;

Client::Client(const std::string& ip, int port)
    : http_ip_(ip), http_port_(port), http_scheme_("http"), e_last_request_(0)
{
    Init();
}

Client::Client(const std::string& url) : e_last_request_(0) {
    auto uri = evhttp_uri_parse(url.c_str());
    if (!uri)
        throw runtime_error("Cannot pars client URL!");
    http_port_ = evhttp_uri_get_port(uri);
    http_ip_ = evhttp_uri_get_host(uri) ? string(evhttp_uri_get_host(uri)) : "";
    http_scheme_ = evhttp_uri_get_scheme(uri) ? string(evhttp_uri_get_scheme(uri)) : "";
    evhttp_uri_free(uri);
    Init();
}

Client::~Client() {
    if (e_base_)
        event_base_loopexit(e_base_, NULL);
#ifdef _WIN32
    WSACleanup();
#endif
    if (e_conn_)
        evhttp_connection_free(e_conn_);
    if (e_dns_)
        evdns_base_free(e_dns_, 0);
    if (e_base_)
        event_base_free(e_base_);
}

void Client::Init() {
#ifdef _WIN32
    {
        /* If running on Windows need to initialise sockets. */
        WORD wVersionRequested;
        WSADATA wsaData;
        wVersionRequested = MAKEWORD(2, 2);
        WSAStartup(wVersionRequested, &wsaData);
    }
#else
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        throw runtime_error("could not init sockets!");
#endif
    // create http_host_
	http_host_ = http_ip_;
	if (http_port_ > 0)
		http_host_ += ":" + to_string(http_port_);

    // start connection
    e_base_ = event_base_new();
    e_dns_ = evdns_base_new(e_base_, 1);

    //evhttp_connection_set_timeout(e_conn_, 5);
    //event_enable_debug_logging(EVENT_DBG_ALL);
}

Request Client::Get(std::string path, Handler handler, bool is_chunked) {
    return CreateRequest(Request::RequestMethod::GET, path, handler, is_chunked);
}

Request Client::Post(std::string path, Handler handler, bool is_chunked) {
    return CreateRequest(Request::RequestMethod::POST, path, handler, is_chunked);
}

Request Client::Put(std::string path, Handler handler, bool is_chunked) {
    return CreateRequest(Request::RequestMethod::PUT, path, handler, is_chunked);
}

Request Client::Patch(std::string path, Handler handler, bool is_chunked) {
    return CreateRequest(Request::RequestMethod::PATCH, path, handler, is_chunked);
}

Request Client::Delete(std::string path, Handler handler, bool is_chunked) {
#undef DELETE
    return CreateRequest(Request::RequestMethod::DELETE, path, handler, is_chunked);
#pragma pop_macro("DELETE")
}

Request Client::CreateRequest(Request::RequestMethod method, std::string path, Handler handler, bool is_chunked) {
    char buf[URL_MAX];
    int r = 0;
    auto uri = evhttp_uri_parse(path.c_str());
	if (uri == NULL)
        throw runtime_error("Invalid path");
    r += evhttp_uri_set_host(uri, http_ip_.c_str());
    r += evhttp_uri_set_port(uri, http_port_);
    r += evhttp_uri_set_scheme(uri, http_scheme_.c_str());
    if (r < 0 || !evhttp_uri_join(uri, buf, URL_MAX)) {
        evhttp_uri_free(uri);
        throw runtime_error("Invalid ip or port");
    }
    evhttp_uri_free(uri);
    string url(buf);
    struct HandleContainer { Handler handler; event_base* e_base; };
    auto cont = new HandleContainer{ handler, e_base_ };
    auto req = evhttp_request_new(
        [](evhttp_request* request, void* handle_ptr) {
            auto cont = static_cast<HandleContainer*>(handle_ptr);
            cont->handler(Response(Request(request), evhttp_request_get_response_code(request)));
            event_base_loopexit(cont->e_base, NULL);
            free(cont);
        }, cont);
    if (is_chunked)
        evhttp_request_set_chunked_cb(req,
            [](evhttp_request* request, void* handle_ptr) {
                auto cont = static_cast<HandleContainer*>(handle_ptr);
                cont->handler(Response(Request(request), evhttp_request_get_response_code(request)));
            });
    evhttp_request_set_error_cb(req,
        [](enum evhttp_request_error err_code, void* handle_ptr) {
            auto cont = static_cast<HandleContainer*>(handle_ptr);
            free(cont);
            switch (err_code)
            {
            case EVREQ_HTTP_TIMEOUT:
                throw runtime_error("Request timeout");
                break;
            case EVREQ_HTTP_EOF:
                throw runtime_error("Request EOF");
                break;
            case EVREQ_HTTP_INVALID_HEADER:
                throw runtime_error("Request invalid header");
                break;
            case EVREQ_HTTP_BUFFER_ERROR:
                throw runtime_error("Request buffer error");
                break;
            case EVREQ_HTTP_REQUEST_CANCEL:
                throw runtime_error("Request cancel");
                break;
            case EVREQ_HTTP_DATA_TOO_LONG:
                throw runtime_error("Request data too long");
                break;
            default:
                break;
            }
        });
    Request request(req, method, url);
    request.PushHeader("Host", http_host_);
    return request;
}

void Client::SendAsyncRequest(Request& request) {
    evhttp_cmd_type m;
#undef DELETE
    switch (request.Method()) {
    case Request::RequestMethod::GET:       m = EVHTTP_REQ_GET; break;
    case Request::RequestMethod::POST:      m = EVHTTP_REQ_POST; break;
    case Request::RequestMethod::HEAD:      m = EVHTTP_REQ_HEAD; break;
    case Request::RequestMethod::PUT:       m = EVHTTP_REQ_PUT; break;
    case Request::RequestMethod::DELETE:    m = EVHTTP_REQ_DELETE; break;
    case Request::RequestMethod::OPTIONS:   m = EVHTTP_REQ_OPTIONS; break;
    case Request::RequestMethod::TRACE:     m = EVHTTP_REQ_TRACE; break;
    case Request::RequestMethod::CONNECT:   m = EVHTTP_REQ_CONNECT; break;
    case Request::RequestMethod::PATCH:     m = EVHTTP_REQ_PATCH; break;
    }
#pragma pop_macro("DELETE")
    e_conn_ = evhttp_connection_base_new(e_base_, e_dns_, http_ip_.c_str(), http_port_);
    evhttp_connection_free_on_completion(e_conn_);
    evhttp_make_request(e_conn_, request.evrequest_, m, request.FullUrl().c_str());
    e_conn_ = NULL;
}

void Client::SendRequest(Request& request) {
    Client::SendAsyncRequest(request);
    int r = event_base_dispatch(e_base_);
}