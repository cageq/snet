#include "tcp_client.h"
#include "user_session.h"
#include <chrono>
#include <thread>




int main(int argc, char *argv[]){

    TcpClient<UserSession> tcpClient; 
  tcpClient.start(); 

    auto conn = tcpClient.connect("127.0.0.1", 8888); 



  
    while(1){

        std::this_thread::sleep_for(std::chrono::seconds(1)); 
        printf("send message\n"); 
        conn->msend("hello"); 

    }

}