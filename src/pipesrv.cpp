#include "pipe_session.h"
#include "tcp_pipe.h"
#include <chrono>
#include <thread>


using namespace snet; 
class UserSession : public PipeSession<UserSession> {



}; 



int main(int argc , char * argv[]){



    TcpPipe<UserSession> tcpPipe; 
    tcpPipe.start("0.0.0.0", 8888, PIPE_SERVER_MODE); 
    tcpPipe.add_local_pipe("1"); 


    while(1){
        std::this_thread::sleep_for(std::chrono::seconds(1)); 
    }

}