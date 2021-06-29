//
// Created by Melodies Sim on 28/6/21.
//
// References: https://github.com/yedf/kqueue-example/blob/master/main.cc
// A simple server side code, which echos back the received message.
// Handles multiple socket connections with kqueue on MacOS

#include <sys/socket.h>
#include <sys/event.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#define PORT 8001
#define exit_if(r, ...) if(r) {printf(__VA_ARGS__); printf("error no: %d error msg %s\n", errno, strerror(errno)); exit(1);}

const int kReadEvent = 1;
const int kWriteEvent = 2;
const int kMaxEvents = 20;

void setNonBlock(int fd);
void updateKEvents(int efd, int fd, int event, bool toDelete);
void loopOnce(int efd, int lfd, timespec timeout);
void handleRead(int efd, int fd);
void handleWrite(int efd, int fd);
void handleAccept(int efd, int fd);

int main(int argc, char *argv[])
{
    int efd, lfd, r;
    int opt = 1;
    struct sockaddr_in addr;

    // Create an epoll file descriptor
    efd = kqueue();
    exit_if(efd < 0, "kqueue failed");

    // Create a listening socket
    lfd = socket(AF_INET, SOCK_STREAM, 0);
    exit_if(lfd < 0, "socket failed");

    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) &&
        setsockopt(lfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    r = bind(lfd, (struct sockaddr *)&addr, sizeof(addr));
    exit_if(r, "bind to 0.0.0.0:%d failed %d %s", PORT, errno, strerror(errno));

    r = listen(lfd, 20);
    exit_if(r, "listen failed %d %s", errno, strerror(errno));
    printf("fd %d listening at %d\n", lfd, PORT);

    setNonBlock(lfd);
    // Add a read event to listening socket
    updateKEvents(efd, lfd, kReadEvent, false);

    int waitMs = 10000;
    struct timespec timeout;
    timeout.tv_sec = waitMs / 1000;
    timeout.tv_nsec = (waitMs % 1000) * 1000 * 1000;

    // Loop indefinitely to handle kevents in kqueue
    for (;;) loopOnce(efd, lfd, timeout);
    return 0;
}

// Set file descriptor to non-blocking
// e.g. reads can proceed even when write is occurring
void setNonBlock(int fd)
{
    int flags = fcntl(fd, F_GETFD, 0);
    exit_if(flags<0, "fcntl failed");
    int r  = fcntl(fd, F_SETFD, flags | O_NONBLOCK);
    exit_if(r<0, "fcntl failed");
}

// Create, update or delete kevents in kqueue efd
// A kevent is identified by an <ident, filter> pair.
// The ident might be a descriptor (file, socket, stream),
// a process ID or a signal number, depending on what we want to monitor.
void updateKEvents(int efd, int fd, int event, bool toDelete)
{
    struct kevent ev[2]; // can set up to 2 kevents
    int n = 0;

    if (event & kReadEvent) {
        EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD|EV_ENABLE, 0, 0, (void*)(intptr_t)fd);
    } else if (toDelete){
        EV_SET(&ev[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, (void*)(intptr_t)fd);
    }
    if (event & kWriteEvent) {
        EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD|EV_ENABLE, 0, 0, (void*)(intptr_t)fd);
    } else if (toDelete){
        EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void*)(intptr_t)fd);
    }

    printf("%s fd %d events read %d write %d\n",
           toDelete ? "del" : "add", fd, event & kReadEvent, event & kWriteEvent);
    int r = kevent(efd, ev, n, NULL, 0, NULL); // add kevent to kqueue
    exit_if(r, "kevent failed");
}

// Handle kevents in kqueue with efd file descriptor
void loopOnce(int efd, int lfd, timespec timeout)
{
    int fd, event, i, n;
    struct kevent activeEvs[kMaxEvents];

    // Monitor triggered events which will be placed into activeEvs
    n = kevent(efd, NULL, 0, activeEvs, kMaxEvents, &timeout);
    printf("epoll_wait return %d\n", n);

    for (i = 0; i < n; i ++)
    {
        fd = (int)(intptr_t)activeEvs[i].udata;
        event = activeEvs[i].filter;
        if (event == EVFILT_READ) {
            if (fd == lfd) {
                handleAccept(efd, fd);
            } else {
                handleRead(efd, fd);
            }
        } else if (event == EVFILT_WRITE) {
            handleWrite(efd, fd);
        } else {
            exit_if(1, "unknown event");
        }
    }
}

void handleAccept(int efd, int fd)
{
    int r;
    struct sockaddr_in raddr, peer, local;
    socklen_t rsz = sizeof(raddr), alen = sizeof(peer);

    int cfd = accept(fd, (struct sockaddr *)&raddr, &rsz);
    exit_if(cfd<0, "accept failed");

    r = getpeername(cfd, (sockaddr*)&peer, &alen);
    exit_if(r<0, "getpeername failed");
    printf("accept a connection from %s\n", inet_ntoa(raddr.sin_addr));

    setNonBlock(cfd);
    updateKEvents(efd, cfd, kReadEvent|kWriteEvent, false);
}

void handleRead(int efd, int fd)
{
    int n = 0, r;
    char buf[4096] = {0};
    while ((n = read(fd, buf, sizeof buf)) > 0)
    {
        printf("read %d bytes\n", n);
        r = write(fd, buf, n); // echo back message read
        exit_if(r<=0, "write error"); // in practice, EAGAIN (no data read) could occur
    }

    if (n<0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return;

    exit_if(n<0, "read error"); // Should check for various errors such as EINTR
    printf("fd %d closed\n", fd);
    close(fd);
}

void handleWrite(int efd, int fd)
{
    //Dummy code place here
    updateKEvents(efd, fd, kReadEvent, true);
}

