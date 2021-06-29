//
// Created by Melodies Sim on 28/6/21.
//
// A simple server side code, which echos back the received message.
// Handles multiple socket connections with kqueue on MacOS

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/event.h>

int main(int argc, char *argv[])
{
    int epfd, nfds;
    struct epoll_event event, events[5];

    // Create the epoll descriptor.
    // Only one is needed per app, and is used to monitor all sockets.
    // The function argument is ignored (it was not before, but now it is),
    // so put your favorite number here
    epfd = epoll_create(1);
}

