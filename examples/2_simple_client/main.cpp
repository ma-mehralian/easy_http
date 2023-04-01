#include <iostream>
#include <easy_http/client.h>
#include <easy_http/server.h>

using namespace std;

int main(int argn, char* argc[]) {
	Client c("http://127.0.0.1:4313");
	auto req = c.SendRequest(Request::RequestMethod::GET, "/api/color_classes");
	cout << "calling:" << req.FullUrl() << endl;
	cout << req.GetContent();
	return 0;
}