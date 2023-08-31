#pragma once 
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <vector> 
#include <arpa/inet.h>

template<class Connection >
class TcpServer{

    public:

        using ConnectionPtr = std::shared_ptr<Connection>; 
        int start(uint16_t port){ 
            listen_sd = socket(AF_INET, SOCK_STREAM, 0);
            if (listen_sd < 0)
            {
                perror("socket() failed");
                exit(-1);
            }    
            this->set_reuse(); 
            this->set_nonblocking(listen_sd); 
            do_bind(port); 
            do_listen(); 
            do_accept(); 
            return 0; 
        }

        void do_accept(){
            int    desc_ready = false ;
            struct timeval       timeout;
            is_running = true ; 
            /*************************************************************/
            /* Initialize the master fd_set                              */
            /*************************************************************/
            FD_ZERO(&master_set);
            max_sd = listen_sd;
            FD_SET(listen_sd, &master_set);

            /*************************************************************/
            /* Initialize the timeval struct to 3 minutes.  If no        */
            /* activity after 3 minutes this program will end.           */
            /*************************************************************/
            timeout.tv_sec  = 3;
            timeout.tv_usec = 0;

            /*************************************************************/
            /* Loop waiting for incoming connects or for incoming data   */
            /* on any of the connected sockets.                          */
            /*************************************************************/
            do
            {
                /**********************************************************/
                /* Copy the master fd_set over to the working fd_set.     */
                /**********************************************************/
                memcpy(&working_set, &master_set, sizeof(master_set));

                /**********************************************************/
                /* Call select() and wait 3 minutes for it to complete.   */
                /**********************************************************/
                // printf("Waiting on select()...\n");
                int rc = select(max_sd + 1, &working_set, NULL, NULL, &timeout);

                /**********************************************************/
                /* Check to see if the select call failed.                */
                /**********************************************************/
                if (rc < 0)
                {
                    perror("  select() failed");
                    break;
                }

                /**********************************************************/
                /* Check to see if the 3 minute time out expired.         */
                /**********************************************************/
                if (rc == 0)
                {
                  //  printf("  select() timed out.\n");
                    continue;
                }

                /**********************************************************/
                /* One or more descriptors are readable.  Need to         */
                /* determine which ones they are.                         */
                /**********************************************************/
                desc_ready = rc;
                for (int fd =0; fd <= max_sd  &&  desc_ready > 0; ++fd)
                {
                    /*******************************************************/
                    /* Check to see if this descriptor is ready            */
                    /*******************************************************/
                    if (FD_ISSET(fd, &working_set))
                    {
                        /****************************************************/
                        /* A descriptor was found that was readable - one   */
                        /* less has to be looked for.  This is being done   */
                        /* so that we can stop looking at the working set   */
                        /* once we have found all of the descriptors that   */
                        /* were ready.                                      */
                        /****************************************************/
                        desc_ready -= 1;

                        /****************************************************/
                        /* Check to see if this is the listening socket     */
                        /****************************************************/
                        if (fd == listen_sd)
                        {
                            printf("Listening socket is readable\n");
                            /*************************************************/
                            /* Accept all incoming connections that are      */
                            /* queued up on the listening socket before we   */
                            /* loop back and call select again.              */
                            /*************************************************/
                            do
                            {
                                /**********************************************/
                                /* Accept each incoming connection.  If       */
                                /* accept fails with EWOULDBLOCK, then we     */
                                /* have accepted all of them.  Any other      */
                                /* failure on accept will cause us to end the */
                                /* server.                                    */
                                /**********************************************/
                                new_sd = accept(listen_sd, NULL, NULL);
                                if (new_sd < 0)
                                {
                                    if (errno != EWOULDBLOCK)
                                    {
                                        perror("accept() failed");
                                        is_running = false; 
                                    }
                                    break;
                                }

                                /**********************************************/
                                /* Add the new incoming connection to the     */
                                /* master read set                            */
                                /**********************************************/
                                printf("New incoming connection - %d\n", new_sd);

                                auto conn = std::make_shared<Connection>(); 
				                connections.push_back(conn); 
                                conn->init(new_sd); 
                                 this->set_nonblocking(new_sd); 

                                FD_SET(new_sd, &master_set);
                                if (new_sd > max_sd)
                                    max_sd = new_sd;

                                /**********************************************/
                                /* Loop back up and accept another incoming   */
                                /* connection                                 */
                                /**********************************************/
                            } while (new_sd != -1);
                        }

                        /****************************************************/
                        /* This is not the listening socket, therefore an   */
                        /* existing connection must be readable             */
                        /****************************************************/
                        else
                        {
                            printf("readable fd %d\n", fd); 
                            auto conn = find_connection(fd); 
                            if (conn){
                                auto ret = conn->do_read(); 
                                if (ret < 0){
                                     FD_CLR(fd, &master_set);
                                }
                            } 
                            
                        } /* End of existing connection is readable */
                    } /* End of if (FD_ISSET(i, &working_set)) */
                } /* End of loop through selectable descriptors */

            } while (is_running ); 

        }
        /*************************************************************/
        /* Allow socket descriptor to be reuseable                   */
        /*************************************************************/
        void set_reuse(){
            int on = 1; 
            int rc = setsockopt(listen_sd, SOL_SOCKET,  SO_REUSEADDR,
                    (char *)&on, sizeof(on));
            if (rc < 0)
            {
                perror("setsockopt() failed");
                ::close(listen_sd);
                exit(-1);
            }
        }

        void set_nonblocking(int fd ){

            /*************************************************************/
            /* Set socket to be nonblocking. All of the sockets for      */
            /* the incoming connections will also be nonblocking since   */
            /* they will inherit that state from the listening socket.   */
            /*************************************************************/
            int on = 1; 
            int rc = ioctl(fd, FIONBIO, (char *)&on);
            if (rc < 0)
            {
                perror("ioctl() failed");
                ::close(fd);
                exit(-1);
            }
        }

        void do_bind(uint16_t port , const char * ipAddr = "0.0.0.0"){

            // struct sockaddr_in6   addr; 
            // memset(&addr, 0, sizeof(addr));
            // addr.sin6_family      = AF_INET6; 
            // memcpy(&addr.sin6_addr, &in6addr_any, sizeof(in6addr_any));
            // addr.sin6_port        = htons(port);

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr(ipAddr);
            addr.sin_port = htons(port);

 
            int rc = bind(listen_sd,(struct sockaddr *)&addr, sizeof(sockaddr_in));
            if (rc < 0)
            {
                perror("bind() failed");
                ::close(listen_sd);
                exit(-1);
            }
        }
        ConnectionPtr find_connection(int fd){
            for(auto & c : connections){
                if (c->conn_sd == fd ){
                    return c; 
                }
            }
            return nullptr; 
        }

        void do_listen(){
            /*************************************************************/
            /* Set the listen back log                                   */
            /*************************************************************/
            int rc = listen(listen_sd, 32);
            if (rc < 0)
            {
                perror("listen() failed");
                ::close(listen_sd);
                exit(-1);
            } 
        }

        struct fd_set        master_set, working_set;
        int    listen_sd, max_sd, new_sd;
        bool is_running = false; 

        std::vector<ConnectionPtr> connections; 
}; 
