#include <iostream>
#include <easy_http/client.h>
#include <easy_http/server.h>

using namespace std;

int main(int argn, char* argc[]) {
	Client c("http://127.0.0.1:4110");
	auto h = [](Request& request) {
		printf("new chunk:\n %s \n", request.GetContent().c_str());
	};

	try {
		auto req = c.SendRequest(Request::RequestMethod::POST, "/engine?service=start");
		cout << "response: " << req.GetContent() << endl;
		auto req2 = c.SendChunkedRequest(h, Request::RequestMethod::GET, "/engine/live");
		cout << "calling:" << req.FullUrl() << endl;
		cout << "response: " << req2.GetContent();
	}
	catch (const exception& e) {
		cout << "request failed: " << e.what() << endl;
	}
	return 0;
}