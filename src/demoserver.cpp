
#include "tcp_server.h"
#include "user_session.h"

int main(int argc, char *argv[])
{

    TcpServer<UserSession> myServer;

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
