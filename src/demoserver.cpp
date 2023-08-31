
#include "tcp_server.h"
#include "user_session.h"

int main(int argc, char * argv[]){



    TcpServer<UserSession> myServer; 

    myServer.start(8888); 


}