#include <easy_http/request.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <regex>
#include <algorithm>
#include <sstream>
#include "utils.h"

using namespace std;

struct EventFree {
    void operator()(evhttp_uri* uri) const { evhttp_uri_free(uri); }
    void operator()(const evhttp_uri* uri) const { /*evhttp_uri_free(uri);*/ }
    void operator()(evkeyvalq* headers) const { evhttp_clear_headers(headers); }
};
template<typename T>
using event_ptr = std::unique_ptr<T, EventFree>;

Request::Request(evhttp_request* request) : evrequest_(request) {
#pragma push_macro("DELETE")
#undef DELETE
    // read reaquest type
    switch (evhttp_request_get_command(request)) {
    case EVHTTP_REQ_GET: method_ = RequestMethod::GET; break;
    case EVHTTP_REQ_POST: method_ = RequestMethod::POST; break;
    case EVHTTP_REQ_HEAD: method_ = RequestMethod::HEAD; break;
    case EVHTTP_REQ_PUT: method_ = RequestMethod::PUT; break;
    case EVHTTP_REQ_DELETE: method_ = RequestMethod::DELETE; break;
    case EVHTTP_REQ_OPTIONS: method_ = RequestMethod::OPTIONS; break;
    case EVHTTP_REQ_TRACE: method_ = RequestMethod::TRACE; break;
    case EVHTTP_REQ_CONNECT: method_ = RequestMethod::CONNECT; break;
    case EVHTTP_REQ_PATCH: method_ = RequestMethod::PATCH; break;
    }
#pragma pop_macro("DELETE")
    
    // read connection info
#if 1
    char* address;
    ev_uint16_t port;
    evhttp_connection_get_peer(evhttp_request_get_connection(request), &address, &port);
    client_ip_ = string(address);
    client_port_ = port;
#else //BOTH METHODS MAKE A SIMILAR RESULTS!
    auto s = evhttp_connection_get_addr(evhttp_request_get_connection(request));
    if (s) {
        char IP[128];
        switch (s->sa_family)
		{
		case AF_INET:  // IPv4:
		{
			const sockaddr_in* sin = reinterpret_cast<const sockaddr_in*>(s);
			evutil_inet_ntop(AF_INET, &(sin->sin_addr), IP, sizeof(IP));
            client_ip_ = string(IP);
			client_port_ = ntohs(sin->sin_port);
			break;
		}
		case AF_INET6:  // IPv6
		{
			const sockaddr_in6* sin = reinterpret_cast<const sockaddr_in6*>(s);
			evutil_inet_ntop(AF_INET6, &(sin->sin6_addr), IP, sizeof(IP));
            client_ip_ = string(IP);
			client_port_ = ntohs(sin->sin6_port);
			break;
		}

		default:
		{
			printf("%s: Unknown socket address family: %d", __FUNCTION__, s->sa_family);
			break;
		}
		}
    }
#endif

    host_ = evhttp_request_get_host(request);

    //--- read request uri
    //event_ptr<evhttp_uri> e_uri(evhttp_uri_parse(evhttp_request_get_uri(request)));
    event_ptr<const evhttp_uri> e_uri(evhttp_request_get_evhttp_uri(request));
    if (e_uri == nullptr) {
        printf("Failed to create evhttp_uri!");
        throw runtime_error("Failed to create evhttp_uri!");
    }
	uri_.full_path = evhttp_request_get_uri(request) ? string(evhttp_uridecode(evhttp_request_get_uri(request), 0, NULL)) : "";
	uri_.path = evhttp_uri_get_path(e_uri.get()) ? string(evhttp_uridecode(evhttp_uri_get_path(e_uri.get()), 0, NULL)) : "";
	uri_.scheme = evhttp_uri_get_scheme(e_uri.get()) ? string(evhttp_uri_get_scheme(e_uri.get())) : "";
	uri_.userinfo = evhttp_uri_get_userinfo(e_uri.get()) ? string(evhttp_uri_get_userinfo(e_uri.get())) : "";
	uri_.port = evhttp_uri_get_port(e_uri.get());
	uri_.fragment = evhttp_uri_get_fragment(e_uri.get()) ? string(evhttp_uri_get_fragment(e_uri.get())) : "";
    
    //--- read request query string
#if 1
    auto qury_char = evhttp_uri_get_query(e_uri.get());
	if (qury_char) {
        string query_str = qury_char;
		std::regex pattern("([^&=]+)=([^&=]*)");
		auto words_begin = std::sregex_iterator(query_str.begin(), query_str.end(), pattern);
		auto words_end = std::sregex_iterator();
		for (std::sregex_iterator i = words_begin; i != words_end; i++){
			std::string key = evhttp_uridecode((*i)[1].str().c_str(), 0, NULL);
			std::string value = evhttp_uridecode((*i)[2].str().c_str(), 0, NULL);
            //--- array params
            if (key.size() > 2 && key.substr(key.size() - 2) == "[]") {
                key = key.substr(0, key.size() - 2);
                if(uri_.query.find(key) != uri_.query.end())
					uri_.query[key] += "," + value;
                else
					uri_.query[key] = value;
			}
			else
				uri_.query[key] = value;
		}
	}
#else
    // DISABLED DUE TO UNKNOW ERROR
    evkeyvalq queries;
    event_ptr<evkeyvalq> queries_ptr(&queries);
    evhttp_parse_query(evhttp_request_get_uri(request), &queries);
    auto q_keyval = queries.tqh_first;
    while (q_keyval){
        request_info.uri_query[q_keyval->key]= q_keyval->value;
        q_keyval = q_keyval->next.tqe_next;
    }
#endif

    // read request headers
    //event_ptr<evkeyvalq> headers(evhttp_request_get_input_headers(request));
    auto headers = evhttp_request_get_input_headers(request);
    auto h_keyval = headers->tqh_first;
    while (h_keyval){
        headers_[h_keyval->key] = h_keyval->value;
        h_keyval = h_keyval->next.tqe_next;
    }

    // read request body
    // note: unique_ptr<evbuffer, EventFree> request_buffer will crash after evbuffer_free
    auto request_buffer = evhttp_request_get_input_buffer(request);
    auto req_body_len = evbuffer_get_length(request_buffer);
    if (req_body_len > 0) {
        body_.resize(req_body_len);
        evbuffer_copyout(request_buffer, (void*)body_.data(), req_body_len);
    }
}

#ifdef USE_JSON
nlohmann::json Request::Json() const {
    try {
        return nlohmann::json::parse(body_);
    }
    catch (const exception e) {
        printf("Invalid json: %s", e.what());
        throw e;
    }
}
#endif // USE_JSON

bool Request::UrlIs(std::string pattern, std::vector<std::string>& vars) const {
	std::smatch base_match;
    if (std::regex_match(uri_.path, base_match, std::regex(pattern))) {
        for (int i = 1; i < base_match.size(); i++)
            vars.push_back(base_match[i].str());
        return true;
    }
    return false;
}

//--- utils functions
bool Request::IsFilled(const std::map<std::string, std::string>& list, std::string key) const {
    return (list.find(key) != list.end()) && !list.at(key).empty();
}

//-----
template<>
std::string Request::GetVal(std::string str_value) const {
    return str_value;
}

template<>
int Request::GetVal(std::string str_value) const {
    return stoi(str_value);
}

template<>
double Request::GetVal(std::string str_value) const {
    return stod(str_value);
}

// time format should be %Y-%m-%d or %Y-%m-%d-%H-%M-%S
template<>
time_t Request::GetVal(std::string str_value) const {
	if (std::count(str_value.begin(), str_value.end(), '-') == 2)
		str_value += "-00-00-00";
	return str2time(str_value, "%Y-%m-%d-%H-%M-%S");
}

template<>
vector<int> Request::GetVal(std::string str_value) const {
	vector<int> result;
	stringstream ss(str_value);
	while (ss.good()) {
		string num;
		getline(ss, num, ',');
		result.push_back(stoi(num));
	}
	return result;
}
