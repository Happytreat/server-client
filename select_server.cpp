//
// Created by Melodies Sim on 28/6/21.
//
// References: https://www.geeksforgeeks.org/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/
// A simple server side code, which echos back the received message.
// Handles multiple socket connections with select and fd_set on Linux

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros

#define TRUE   1
#define FALSE  0
#define PORT 8888

int main(int argc, char *argv[])
{
    int master_socket, addrlen, new_socket, client_socket[30],
        max_clients = 30, activity, i, valread, sd;
    int opt = 1;
    int max_sd;
    struct sockaddr_in address;
    const char *message = "Hello from server.";
    char buffer[1025];

    fd_set readfds;

    // Initialise all client_socket[] to 0
    for (i = 0; i < max_clients; i++)
    {
        client_socket[i] = 0;
    }

    // Create listening socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) &&
        setsockopt(master_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Bind server to localhost
    address.sin_port = htons(PORT);

    // Bind socket to localhost port
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) > 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener on port %d \n", PORT);

    // Specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    addrlen = sizeof(address);
    puts("Waiting for connections ...");

    while(TRUE)
    {
        // Clear the socket set
        FD_ZERO(&readfds);

        // Add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // Add child sockets to set
        for ( i = 0 ; i < max_clients ; i++)
        {
            // Socket descriptor
            sd = client_socket[i];

            // If valid socket descriptor then add to read list
            if(sd > 0) FD_SET( sd , &readfds);

            // Highest file descriptor number, need it for the select function
            if(sd > max_sd) max_sd = sd;
        }

        // Wait for an activity on one of the sockets , timeout is NULL ,
        // so wait indefinitely
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno!=EINTR))
        {
            printf("select error");
        }

        // Handle listening socket (master) differently from
        // accepted sockets (client)
        if (FD_ISSET(master_socket, &readfds))
        {
            if ((new_socket = accept(master_socket,
                                     (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            printf("New connection , socket fd is %d , ip is : %s , port : %d"
                   "\n" , new_socket , inet_ntoa(address.sin_addr) , ntohs
                    (address.sin_port));

            //send new connection greeting message
            if( send(new_socket, message, strlen(message), 0) != strlen(message) )
            {
                perror("send");
            }

            puts("Welcome message sent successfully");

            //add new socket to array of sockets
            for (i = 0; i < max_clients; i++)
            {
                //if position is empty
                if( client_socket[i] == 0 )
                {
                    client_socket[i] = new_socket;
                    printf("Adding to list of sockets as %d\n" , i);

                    break;
                }
            }
        }

        // Else its some IO operation on soem accepted socket
        for (i = 0; i < max_clients; i++)
        {
            sd = client_socket[i];

            if (FD_ISSET(sd , &readfds))
            {
                // Check if it was for closing , and also read the
                // incoming message
                if ((valread = read( sd , buffer, 1024)) == 0)
                {
                    // Somebody disconnected, get his details and print
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n",
                           inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

                    // Close the socket and mark as 0 in list for reuse
                    close(sd);
                    client_socket[i] = 0;
                }
                else //Echo back the message that came in
                {
                    // Set the string terminating NULL byte on the end
                    // of the data read
                    buffer[valread] = '\0';
                    send(sd , buffer , strlen(buffer) , 0);
                }
            }
        }
    }
}