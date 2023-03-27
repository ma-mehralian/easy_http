#include <easy_http/client.h>
#include <signal.h>
#include <event2/event.h>
#include <event2/http.h>

#ifdef EVENT__HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef EVENT__HAVE_AFUNIX_H
#include <afunix.h>
#endif

using namespace std;

Client::Client(const std::string& ip, int port)
    : http_ip_(ip), http_port_(port), e_last_request_(0) 
{
    Init();
}

Client::Client(const std::string& url) : e_last_request_(0) {
    auto uri = evhttp_uri_parse(url.c_str());
    http_port_ = evhttp_uri_get_port(uri);
    http_ip_ = evhttp_uri_get_host(uri) ? string(evhttp_uri_get_host(uri)) : "";
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
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        return -1;
    }
#endif
    e_base_ = event_base_new();
    e_conn_ = evhttp_connection_base_new(e_base_, NULL, http_ip_.c_str(), http_port_);
    evhttp_connection_set_timeout(e_conn_, 5);
}

Request Client::SendRequest(Request::RequestMethod method, std::string path) {
    auto e_request = evhttp_request_new(&Client::ResponseHandler, this);
	auto e_headers = evhttp_request_get_output_headers(e_request);
	evhttp_add_header(e_headers, "Host", http_ip_.c_str());

    evhttp_cmd_type m = EVHTTP_REQ_GET;
#undef DELETE
    switch (method) {
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
    evhttp_make_request(e_conn_, e_request, m, path.c_str());
    event_base_dispatch(e_base_);
    if (!e_last_request_)
        throw runtime_error("Request failed");
    return Request(e_last_request_);
}

void Client::ResponseHandler(evhttp_request* request, void* client_ptr) {
    auto client = static_cast<Client*>(client_ptr);
    client->e_last_request_ = request;
    if(request)
        evhttp_request_own(request);
    event_base_loopbreak(client->e_base_);
}