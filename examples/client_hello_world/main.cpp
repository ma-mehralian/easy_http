#include <iostream>
#include <easy_http/client.h>
#include <easy_http/response.h>
#ifdef USE_MLD
#include <vld.h>
#endif

int main(int argn, char* argc[]) {
	Client c("http://localhost:4313/");
	try {
		auto req = c.Get("/api/color_classes",
			[&](const Response &res) {
				auto data = res.GetContent();
				std::cout << "new response: " << data << std::endl;
			});
		req.PushHeader("TEST_header", "test_value");
		std::cout << "calling: " << req.FullUrl() << std::endl;
		req.Send();
	}
	catch (const std::exception& e) {
		std::cout << "request failed: " << e.what() << std::endl;
	}
	return 0;
}