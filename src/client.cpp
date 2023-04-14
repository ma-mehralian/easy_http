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

#define URL_MAX 4096

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
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        throw runtime_error("could not init sockets!");
#endif
    e_base_ = event_base_new();
    e_conn_ = evhttp_connection_base_new(e_base_, NULL, http_ip_.c_str(), http_port_);
    //evhttp_connection_set_timeout(e_conn_, 5);
}

Request Client::CreateRequest(Request::RequestMethod method, std::string path, const HeaderList& headers) {
    char buf[URL_MAX];
    int r = 0;
    auto uri = evhttp_uri_parse(path.c_str());
	if (uri == NULL)
        throw runtime_error("Invalid path");
    r += evhttp_uri_set_host(uri, http_ip_.c_str());
    r += evhttp_uri_set_port(uri, http_port_);
    if (r < 0 || !evhttp_uri_join(uri, buf, URL_MAX)) {
        evhttp_uri_free(uri);
        throw runtime_error("Invalid ip or port");
    }
    evhttp_uri_free(uri);
    string url(buf);
    auto req = evhttp_request_new(&Client::ResponseHandler, this);
    Request request(req, method, url);
    request.SetHeaders(headers);
    return request;
}

Request Client::SendRequest(Request& request) {
    chunk_handler_ = nullptr;
    return MakeRequest(request);
}

void Client::SendChunkedRequest(std::function<void(const Request&)> h, Request& request) {
    chunk_handler_ = h;
    evhttp_request_set_chunked_cb(request.evrequest_, &Client::ChunkedResponseHandler);
    return MakeAsyncRequest(request);
}

void Client::MakeAsyncRequest(Request& request) {
    error_code_ = -1;
    e_last_request_ = nullptr;
    evhttp_request_set_error_cb(request.evrequest_,
        [](enum evhttp_request_error err_code, void* client_ptr) {
            Client::ResponseErrorHandler(err_code, client_ptr);
        });
    request.PushHeader("Host", http_ip_);

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
    evhttp_make_request(e_conn_, request.evrequest_, m, request.FullUrl().c_str());
}

Request Client::MakeRequest(Request& request) {
    MakeAsyncRequest(request);
    int r = event_base_dispatch(e_base_);
    if (error_code_ != -1) {
        switch ((evhttp_request_error)error_code_)
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
    }
    if (!e_last_request_)
        throw runtime_error("Request failed");
    else{
		return Request(e_last_request_);
    }
}

void Client::ResponseHandler(evhttp_request* request, void* client_ptr) {
    auto client = static_cast<Client*>(client_ptr);
    client->e_last_request_ = request;
    if(request)
        evhttp_request_own(request);
    event_base_loopbreak(client->e_base_);
}

void Client::ChunkedResponseHandler(struct evhttp_request* request, void* client_ptr) {
    auto client = static_cast<Client*>(client_ptr);
    if (client->chunk_handler_) 
        client->chunk_handler_(Request(request));
}

void Client::ResponseErrorHandler(int err_code, void* client_ptr) {
    auto client = static_cast<Client*>(client_ptr);
    client->error_code_ = err_code;
}