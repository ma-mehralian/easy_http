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
    : http_host_(ip), http_port_(port), http_scheme_("http")
{
    char buf[URL_MAX];
    int r = 0;
    auto uri = evhttp_uri_new();
    r += evhttp_uri_set_host(uri, ip.c_str());
    r += evhttp_uri_set_port(uri, port);
    r += evhttp_uri_set_scheme(uri, "http");
    if (r < 0 || !evhttp_uri_join(uri, buf, URL_MAX)) {
        evhttp_uri_free(uri);
        throw runtime_error("Invalid ip or port");
    }
    Init();
}

Client::Client(const std::string& host) {
    auto uri = evhttp_uri_parse(host.c_str());
    if (!uri)
        throw runtime_error("Cannot pars client host!");
	http_port_ = evhttp_uri_get_port(uri) == -1 ? 80 : evhttp_uri_get_port(uri);
    if (evhttp_uri_get_scheme(uri) == NULL)
        throw invalid_argument("URL scheme was not set!");
    else
		http_scheme_ = evhttp_uri_get_scheme(uri);
    if (evhttp_uri_get_host(uri) == NULL)
        throw invalid_argument("Invalid host!");
    else
		http_host_ = evhttp_uri_get_host(uri) ? string(evhttp_uri_get_host(uri)) : "";
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
    // start connection
    e_base_ = event_base_new();
    e_dns_ = evdns_base_new(e_base_, 1);
    e_conn_ = NULL;

    //evhttp_connection_set_timeout(e_conn_, 5);
    //event_enable_debug_logging(EVENT_DBG_ALL);
}

Request Client::Get(std::string path, RequestBase::ResponseHandler handler, bool is_chunked) {
    return CreateRequest(RequestBase::RequestMethod::GET, path, handler, is_chunked);
}

Request Client::Post(std::string path, RequestBase::ResponseHandler handler, bool is_chunked) {
    return CreateRequest(RequestBase::RequestMethod::POST, path, handler, is_chunked);
}

Request Client::Put(std::string path, RequestBase::ResponseHandler handler, bool is_chunked) {
    return CreateRequest(RequestBase::RequestMethod::PUT, path, handler, is_chunked);
}

Request Client::Patch(std::string path, RequestBase::ResponseHandler handler, bool is_chunked) {
    return CreateRequest(RequestBase::RequestMethod::PATCH, path, handler, is_chunked);
}

Request Client::Delete(std::string path, RequestBase::ResponseHandler handler, bool is_chunked) {
#undef DELETE
    return CreateRequest(RequestBase::RequestMethod::DELETE, path, handler, is_chunked);
#pragma pop_macro("DELETE")
}

Request Client::CreateRequest(RequestBase::RequestMethod method, std::string path, RequestBase::ResponseHandler handler, bool is_chunked) {
    char buf[URL_MAX];
    int r = 0;
    auto uri = evhttp_uri_parse(path.c_str());
    if (uri == NULL)
        throw runtime_error("Invalid path");
    r += evhttp_uri_set_host(uri, http_host_.c_str());
    r += evhttp_uri_set_port(uri, http_port_);
    r += evhttp_uri_set_scheme(uri, http_scheme_.c_str());
    if (r < 0 || !evhttp_uri_join(uri, buf, URL_MAX)) {
        evhttp_uri_free(uri);
        throw runtime_error("Could not set host info!");
    }
    evhttp_uri_free(uri);
    string url(buf);
    auto e_conn = evhttp_connection_base_new(e_base_, e_dns_, http_host_.c_str(), http_port_);
    evhttp_connection_free_on_completion(e_conn);
    Request request(e_conn, method, url, handler, is_chunked);
    request.PushHeader("Host", http_host_);
    return request;
}
