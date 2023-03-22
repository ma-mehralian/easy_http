#include <easy_http/middleware.h>
#include <easy_http/controller.h>

using namespace std;

Response Middleware::CallHandler(Request& request) {
	if (next_)
		return Handle(request, bind(&Middleware::CallHandler, next_.get(), placeholders::_1));
	else
		return Handle(request, bind(&Controller::CallHandler, controller_, placeholders::_1));
}
