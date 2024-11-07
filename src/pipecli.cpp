/*
 * Author: wkui
 * Date: 2024-10-15
 * Desc: a multi raft lib
 */
#include "pipe_session.h"
#include "tcp_pipe.h"


using namespace snet; 


class UserSession : public PipeSession<UserSession> {



}; 



int main(int argc , char * argv[]){



    TcpPipe<UserSession> tcpPipe; 
    tcpPipe.start("", 0, PIPE_CLIENT_MODE); 
    tcpPipe.add_remote_pipe("1", "127.0.0.1", 8888); 


    getchar(); 

}