#include <easy_http/server.h>
#include <signal.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <event2/buffer.h>
#include <regex>

#ifdef USE_SPDLOG
#include <spdlog/sinks/null_sink.h>
#define LOG_ERROR(ptr, ...) ptr->logger_->error(##__VA_ARGS__)
#define LOG_INFO(ptr, ...) ptr->logger_->info(##__VA_ARGS__)
#define LOG_DEBUG(ptr, ...) ptr->logger_->debug(##__VA_ARGS__)
#else
#define LOG_ERROR(...) 
#define LOG_INFO(...) 
#define LOG_DEBUG(...) 
#endif //USE_SPDLOG

#ifdef EVENT__HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef EVENT__HAVE_AFUNIX_H
#include <afunix.h>
#endif

using namespace std;

Server::Server(const std::string& ip, int port) :
	http_ip_(ip), http_port_(port),is_started_(false), e_base_(nullptr), e_http_server_(nullptr)
{
#ifdef USE_SPDLOG
    logger_ = spdlog::create<spdlog::sinks::null_sink_st>("null_logger");
#endif //USE_SPDLOG
}

Server::~Server() {
    Stop();
}


static void do_term(evutil_socket_t sig, short events, void* arg)
{
    struct event_base* base = (event_base*)arg;
    event_base_loopbreak(base);
    fprintf(stderr, "Got %Ii, Terminating\n", sig);
}

static int display_listen_sock(struct evhttp_bound_socket* handle)
{
    struct sockaddr_storage ss;
    evutil_socket_t fd;
    ev_socklen_t socklen = sizeof(ss);
    char addrbuf[128];
    void* inaddr;
    const char* addr;
    int got_port = -1;

    fd = evhttp_bound_socket_get_fd(handle);
    memset(&ss, 0, sizeof(ss));
    if (getsockname(fd, (struct sockaddr*)&ss, &socklen)) {
        perror("getsockname() failed");
        return 1;
    }

    if (ss.ss_family == AF_INET) {
        got_port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
        inaddr = &((struct sockaddr_in*)&ss)->sin_addr;
    }
    else if (ss.ss_family == AF_INET6) {
        got_port = ntohs(((struct sockaddr_in6*)&ss)->sin6_port);
        inaddr = &((struct sockaddr_in6*)&ss)->sin6_addr;
    }
#ifdef EVENT__HAVE_STRUCT_SOCKADDR_UN
    else if (ss.ss_family == AF_UNIX) {
        printf("Listening on <%s>\n", ((struct sockaddr_un*)&ss)->sun_path);
        return 0;
    }
#endif
    else {
        fprintf(stderr, "Weird address family %d\n",
            ss.ss_family);
        return 1;
    }

    addr = evutil_inet_ntop(ss.ss_family, inaddr, addrbuf,
        sizeof(addrbuf));
    if (addr) {
        printf("Listening on %s:%d\n", addr, got_port);
        char uri_root[512];
        evutil_snprintf(uri_root, sizeof(uri_root),
            "http://%s:%d", addr, got_port);
    }
    else {
        fprintf(stderr, "evutil_inet_ntop failed\n");
        return 1;
    }

    return 0;
}


// \TODO check event_config, EVTHREAD_USE_WINDOWS_THREADS_IMPLEMENTED, EVENT_BASE_FLAG_STARTUP_IOCP
// https://github.com/libevent/libevent/blob/master/sample/http-server.c
int Server::Start() {
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

    auto  e_cfg = event_config_new();
#ifdef _WIN32
    bool iocp = false;
    if (iocp) {
#ifdef EVTHREAD_USE_WINDOWS_THREADS_IMPLEMENTED
        evthread_use_windows_threads();
        event_config_set_num_cpus_hint(cfg, 8);
#endif
        event_config_set_flag(e_cfg, EVENT_BASE_FLAG_STARTUP_IOCP);
    }
#endif

    e_base_ = event_base_new_with_config(e_cfg);
    if (!e_base_) {
        LOG_ERROR(this, "Couldn't create an event_base");
        return -1;
    }

    e_http_server_ = evhttp_new(e_base_);
    if (!e_http_server_) {
        LOG_ERROR(this, "couldn't create evhttp");
        return -1;
    }

    evhttp_set_allowed_methods(e_http_server_,
        EVHTTP_REQ_GET | EVHTTP_REQ_POST | EVHTTP_REQ_OPTIONS |
        EVHTTP_REQ_PUT | EVHTTP_REQ_DELETE | EVHTTP_REQ_PATCH);

    // Set a callback for all requests
    evhttp_set_gencb(e_http_server_, &Server::RequestHandler, this);
    evhttp_set_max_body_size(e_http_server_, 6410241024);

    auto handle = evhttp_bind_socket_with_handle(e_http_server_, http_ip_.c_str(), http_port_);
    if (!handle) {
        LOG_ERROR(this, "Failed to start HTTP server on {}:{}", http_ip_, http_port_);
        return -1;
    }
    if (display_listen_sock(handle)) {
        return -1;
    }

    auto e_term = evsignal_new(e_base_, SIGINT, do_term, e_base_);
    if (!e_term)
        return -1;
    if (event_add(e_term, NULL))
        return -1;
    LOG_INFO(this, "HTTP server on http://{}:{} started", http_ip_, http_port_);
    is_started_ = true;
    event_base_dispatch(e_base_);
    event_config_free(e_cfg);
    event_free(e_term);
    return 0;
}

int Server::Stop() {
    if (!is_started_)
        return 0;

	if (e_base_)
		event_base_loopexit(e_base_, NULL);
#ifdef _WIN32
	WSACleanup();
#endif
	if (e_http_server_)
		evhttp_free(e_http_server_);
	if (e_base_)
		event_base_free(e_base_);
	return 0;
}

Route& Server::Get(std::string url, Handler handler) {
    routes_.push_back(Route::Get(url, handler));
    return routes_.back();
}

Route& Server::Post(std::string url, Handler handler) {
    routes_.push_back(Route::Post(url, handler));
    return routes_.back();
}

Route& Server::Put(std::string url, Handler handler) {
    routes_.push_back(Route::Put(url, handler));
    return routes_.back();
}

Route& Server::Patch(std::string url, Handler handler) {
    routes_.push_back(Route::Patch(url, handler));
    return routes_.back();
}

Route& Server::Delete(std::string url, Handler handler) {
    routes_.push_back(Route::Delete(url, handler));
    return routes_.back();
}

void Server::RegisterController(std::unique_ptr<Controller> c) {
    controllers_.push_back(std::move(c));
}

#ifdef USE_SPDLOG
void Server::SetLogger(std::shared_ptr<spdlog::logger> logger) {
    logger_ = logger;
}
#endif //USE_SPDLOG

void Server::RequestHandler(evhttp_request* request, void* server_ptr) {
    auto server = static_cast<Server*>(server_ptr);
    Request http_request(request);
    LOG_DEBUG(server, "new request[{}] for {} from {}:{}", (int)http_request.Method(),
        http_request.Url(), http_request.ClientIp(), http_request.ClientPort());
	auto http_response = server->RequestHandler(http_request);
	if (http_response.Send() == 0)
		LOG_DEBUG(server, "response[{}] sent for {}", http_response.GetStatusCode(), http_request.Url());
}

Response Server::RequestHandler(Request& request) {
    // check route urls
    for (auto& r : routes_)
        if (request.UrlIs(r.Url()))
            return r.CallHandler(request);

    // check controller urls
    for (auto& c : controllers_)
        if (c->IsMatch(request))
			return c->Handle(request);
	return Response(request, 404);
}
