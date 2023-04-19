#include <iostream>
#include <easy_http/client.h>
#include <easy_http/server.h>

using namespace std;

int main(int argn, char* argc[]) {
	Client c("127.0.0.1", 4110);

	try {
		auto req1 = c.Post("/engine?service=start",
			[](const Response& res) {
				cout << "response[" << res.GetStatusCode() << "]: " << res.GetContent() << endl;
			});
		cout << "calling:" << req1.FullUrl() << endl;
		c.SendRequest(req1);

		auto req2 = c.Get("/engine/live",
			[](const Response& res) {
				cout << "chunked_response[" << res.GetStatusCode() << "]: " << res.GetContent().substr(0,50) << endl;
			}, true);
		cout << "calling:" << req2.FullUrl() << endl;
		c.SendAsyncRequest(req2);

		printf("Request sent...");
		req2.Wait();
	}
	catch (const exception& e) {
		cout << "request failed: " << e.what() << endl;
	}
	return 0;
}