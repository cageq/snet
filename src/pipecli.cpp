#include "pipe_session.h"
#include "tcp_pipe.h"
class UserSession : public PipeSession<UserSession> {



}; 



int main(int argc , char * argv[]){



    TcpPipe<UserSession> tcpPipe; 
    tcpPipe.add_remote_pipe("1", "127.0.0.1", 8888); 





}