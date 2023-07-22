#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include <easy_http/route.h>
#include <easy_http/controller.h>
#include <functional>
#ifdef USE_SPDLOG
#include <spdlog/spdlog.h>
#endif //USE_SPDLOG

class Server {
public:
	Server(const std::string &ip, int port);
	~Server();

	//since we pass Server pointer to request handler so the current object pointer should not be changed
	Server(const Server&) = delete;
	Server& operator= (const Server&) = delete;

	//! start http server
	virtual int Start();

	//! stop http server
	virtual int Stop();

	Route& Get(std::string url, RequestBase::RequestHandler handler);

	Route& Post(std::string url, RequestBase::RequestHandler handler);

	Route& Put(std::string url, RequestBase::RequestHandler handler);

	Route& Patch(std::string url, RequestBase::RequestHandler handler);

	Route& Delete(std::string url, RequestBase::RequestHandler handler);

	/*!
	 * call in this way:
	 * RegisterController(std::move(crt_ptr));
	 * RegisterController(std::make_unique<WebController>("/url/"));
	 */
	void RegisterController(std::unique_ptr<Controller> c);

#ifdef USE_SPDLOG
	//! set spdlog output
	void SetLogger(std::shared_ptr<spdlog::logger> logger);
#endif //USE_SPDLOG

protected:
	//! Http server event handler
	virtual Response RequestHandler(const Request& request);

	std::vector<std::unique_ptr<Controller>> controllers_;
#ifdef USE_SPDLOG
	std::shared_ptr<spdlog::logger> logger_;
#endif //USE_SPDLOG

private:
	//! libevent request handler
	static void RequestHandler(struct evhttp_request* request, void* server_ptr);

	bool is_started_;
	std::string http_ip_;
	uint16_t http_port_;

	struct event_base* e_base_;
	struct evhttp* e_http_server_;
	
	std::vector<Route> routes_;
};

#endif // !_HTTP_SERVER_H_
