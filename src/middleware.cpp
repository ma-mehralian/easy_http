#include <easy_http/middleware.h>
#include <easy_http/route.h>

using namespace std;

Response Middleware::CallHandler(const Request& request) {
	if (next_)
		return Handle(request, bind(&Middleware::CallHandler, next_.get(), placeholders::_1));
	else
		return Handle(request, last_);
}
