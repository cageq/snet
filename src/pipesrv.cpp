#include "pipe_session.h"
#include "tcp_pipe.h"
class UserSession : public PipeSession<UserSession> {



}; 



int main(int argc , char * argv[]){



    TcpPipe<UserSession> tcpPipe; 
    tcpPipe.start("0.0.0.0", 8888); 
    tcpPipe.add_local_pipe("1"); 



    getchar(); 

}