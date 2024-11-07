/*
 * Author: wkui
 * Date: 2024-10-15
 * Desc: a multi raft lib
 */

#include "tcp_listener.h"
#include "user_session.h"


using namespace snet; 

int main(int argc, char *argv[])
{

    TcpListener<UserSession> myServer;

    myServer.start(8888);

    auto tid = myServer.start_timer([]()
                                    {

//        printf("my main timer timeout\n"); 
        return true; }, 1000000);

    while (1)
    {
        auto ch = getchar();
        if (ch == 's')
        {
            myServer.broadcast("hello from server");
        }
        else if (ch == 'q')
        {

            printf("quiting ............\n");
            break;
        }
    }
}
