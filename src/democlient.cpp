/*
 * Author: wkui
 * Date: 2024-10-15
 * Desc: a multi raft lib
 */
#include "tcp_connector.h"
#include "user_session.h"
#include <chrono>
#include <thread>


using namespace snet; 


int main(int argc, char *argv[]){

	TcpConnector<UserSession> tcpClient; 
	tcpClient.start(); 

	auto conn = tcpClient.connect("127.0.0.1", 8888); 
	conn->enable_reconnect(3000000); 

	while(1){

		std::this_thread::sleep_for(std::chrono::seconds(1)); 
		
		auto ret =  conn->msend("hello"); 
		printf("send message result %d\n", ret ); 

	}
}
