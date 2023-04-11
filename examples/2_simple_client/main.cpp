#include <iostream>
#include <easy_http/client.h>
#include <easy_http/server.h>

using namespace std;

int main(int argn, char* argc[]) {
	Client c("127.0.0.1", 4110);
	auto h = [](const Request& request) {
		printf("new chunk:\n %s \n", request.GetContent().c_str());
	};

	try {
		auto req1 = c.CreateRequest(Request::RequestMethod::POST, "/engine?service=start");
		cout << "calling:" << req1.FullUrl() << endl;
		auto res1 = c.SendRequest(req1);
		cout << "response: " << res1.GetContent() << endl;

		auto req2 = c.CreateRequest(Request::RequestMethod::GET, "/engine/live");
		cout << "calling:" << req2.FullUrl() << endl;
		auto res2 = c.SendChunkedRequest(h, req2);
		cout << "response: " << res2.GetContent();
	}
	catch (const exception& e) {
		cout << "request failed: " << e.what() << endl;
	}
	return 0;
}