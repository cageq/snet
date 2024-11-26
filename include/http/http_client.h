#pragma once

#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream> 
#include "snet.h"
#include "http_request.h"
#include "http_url.h"
#include "http_connection.h"

using namespace snet::tcp;
namespace snet {
namespace http {

class HttpClient : public TcpFactory<HttpConnection> {

public:
	using HttpConnector = TcpConnector<HttpConnection, HttpClient>;

	HttpClient()
		: connector(this) {
		connector.start();
	}

	virtual void handle_event(ConnectionPtr conn, NetEvent evt) {
		switch (evt) {
		case NetEvent::EVT_CONNECT:
			conn->send_first_request() ;
			break;
		case NetEvent::EVT_DISCONNECT:
			connections.erase(conn->get_cid());
			break;

		default:;
		}
	}

	bool get(const std::string& url) {

		HttpUrl httpUrl(url);

		std::cout << httpUrl << std::endl;

		auto req = std::make_shared<HttpRequest>(HttpMethod::HTTP_GET, url); 
		
		NetUrl urlInfo("tcp", httpUrl.host(), httpUrl.port()) ; 
 
		auto conn = connector.add_connection(urlInfo, req );
		

		connections[conn->get_cid()] = conn;
		return true;
	}

private:
	std::unordered_map<uint64_t, std::shared_ptr<HttpConnection>> connections;
	TcpConnector<HttpConnection, HttpClient> connector;
};

} // namespace http
} // namespace snet
