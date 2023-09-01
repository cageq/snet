#include "tcp_client.h"
#include "user_session.h"




int main(int argc, char *argv[]){

    TcpClient<UserSession> tcpClient; 


    tcpClient.connect("127.0.0.1", 8888); 



    tcpClient.run(); 


}