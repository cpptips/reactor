#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_LENGTH 1024
#define MAX_EPOLL_EVENTS (1024 * 500)  // connection
#define MAX_EPOLL_ITEM 1024            // con
#define SERVER_PORT 8888

#define LISTEN_PORT_COUNT 100
typedef int (*NCALLBACK)(int, int, void *);
// 事件
struct event_t {
    int fd;
    int events;  //可读可写的事件
    void *arg;
    int (*callback)(int fd, int events, void *arg);

    int status;
    char buffer[BUFFER_LENGTH];  //缓冲区
    int length;                  // 数据实际长度
    long last_active;
};
// 反应堆
struct reactor_t {
    int epfd;
    struct event_t *events;
};
int recv_cb(int fd, int events, void *arg);
int send_cb(int fd, int events, void *arg);
int reactor_init(struct reactor_t *reactor) {
    if (reactor == NULL) return -1;
    memset(reactor, 0, sizeof(struct reactor_t));

    // 创建epoll
    reactor->epfd = epoll_create(1);
    if (reactor->epfd <= 0) {
        printf("Failed to create epoll\n");
        return -2;
    }

    reactor->events =
        (struct event_t *)malloc(sizeof(struct event_t) * MAX_EPOLL_EVENTS);
    if (reactor->events == NULL) {
        printf("Failed to allocate\n");
        close(reactor->epfd);
        return -3;
    }
}

int init_sock(short port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(fd, F_SETFL, O_NONBLOCK);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);
    server_addr.sin_port = htons(port);

    bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (listen(fd, 20) == -1) {
        printf("Failed to listen\n");
    }
    printf("listen port :%d\n", port);
    return fd;
}

int reactor_addlistener(struct reactor_t *reactor, int sockfd,
                        NCALLBACK acceptor) {
    if (reactor == NULL) return -1;
    if (reactor->events == NULL) return -1;

    event_set(&reactor->events[sockfd], sockfd, acceptor, reactor);
    event_add(reactor->epfd, EPOLLIN, &reactor->events[sockfd]);

    return 0;
}

void event_set(struct event_t *ev, int fd, NCALLBACK callback, void *arg) {
    ev->fd = fd;
    ev->callback = callback;
    ev->events = 0;
    ev->arg = arg;
    ev->last_active = time(NULL);
    return;
}

void event_add(int epfd, int events, struct event_t *ev) {
    struct epoll_event ep_ev = {0, {0}};
    ep_ev.data.ptr = ev;
    ep_ev.events = ev->events = events;  // EPOLLUIN / EPOLLOUT
    int op;
    if (ev->status == 1) {
        op = EPOLL_CTL_MOD;
    } else {
        op = EPOLL_CTL_ADD;
        ev->status = 1;
    }

    if (epoll_ctl(epfd, op, ev->fd, &ep_ev) < 0) {
        printf("event add failed [fd=%d], events[%d]\n", ev->fd, events);
        return -1;
    }

    return 0;
}

int event_del(int epfd, struct event_t *ev) {
    struct epoll_event ep_ev = {0, {0}};
    if (ev->status != 1) {
        return -1;
    }
    ep_ev.data.ptr = ev;
    ev->status = 0;
    epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, &ep_ev);
    return 0;
}

int recv_cb(int fd, int events, void *arg) {
    struct reactor_t *reactor = (struct reactor_t *)arg;
    struct event_t *ev = reactor->events + fd;

    int len = recv(fd, ev->buffer, BUFFER_LENGTH, 0);
    event_del(reactor->epfd, ev);
    if (len > 0) {
        ev->length = len;
        ev->buffer[ev->length] = 0;
        printf("C[%d]:%s\n", fd, ev->buffer);
        event_set(ev, fd, send_cb, reactor);
        event_add(reactor->epfd, EPOLLOUT, ev);
    } else if (len == 0) {
        close(ev->fd);
        printf("[fd=%d] pos[%ld], closed\n", fd, ev - reactor->events);
    } else {
        close(ev->fd);
        printf("recv[fd=%d] error[%d]:%s\n", fd, errno, strerror(errno));
    }
    return len;
}
int send_cb(int fd, int events, void *arg) {
    struct reactor_t *reactor = (struct reactor_t *)arg;
    struct event_t *ev = reactor->events + fd;

    int len = send(fd, ev->buffer, ev->length, 0);
    if (len > 0) {
        printf("send[fd=%d], [%d]%s\n", fd, len, ev->buffer);
        event_del(reactor->epfd, ev);
        event_set(ev, fd, recv_cb, reactor);
        event_add(reactor->epfd, EPOLLIN, ev);
    } else {
        close(ev->fd);
        event_del(reactor->epfd, ev);
        printf("send[fd=%d] error %s\n", fd, strerror(errno));
    }
    return len;
}

int accept_cb(int fd, int events, void *arg) {
    struct reactor_t *reactor = (struct reactor_t *)arg;
    if (reactor == NULL) return -1;

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    int clientfd;
    if ((clientfd = accept(fd, (struct sockaddr *)&client_addr, &len)) == -1) {
        if (errno != EAGAIN || errno != EINTR) {
        }
        printf("accept: %s\n", strerror(errno));
        return -1;
    }

    int i = 0;
    do {
        int flag = 0;
        if ((flag = fcntl(clientfd, F_SETFL, O_NONBLOCK)) < 0) {
            printf("fcntl error\n");
            break;
        }
        event_set(&reactor->events[clientfd], clientfd, recv_cb, reactor);
        event_add(reactor->epfd, EPOLLIN, &reactor->events[clientfd]);
    } while (0);
    printf("new connect [%s:%d][time:%ld], pos[%d]\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
           reactor->events[i].last_active, i);
    return 0;
}

int reactor_destroy(struct reactor_t *reactor) {
    close(reactor->epfd);
    free(reactor->events);
    return 0;
}

int reactor_run(struct reactor_t *reactor) {
    if (reactor == NULL) return -1;
    if (reactor->epfd < -1) return -1;
    if (reactor->events == NULL) return -1;

    struct epoll_event events[MAX_EPOLL_ITEM];
    int checkpos = 0;
    int i;
    while (1) {
        int nready = epoll_wait(reactor->epfd, events, MAX_EPOLL_ITEM, 1000);
        if (nready < 0) {
            printf("Failed to epoll_wait\n");
            continue;
        }

        for (i = 0; i < nready; i++) {
            struct event_t *ev = (struct event_t *)events[i].data.ptr;
            if ((events[i].events & EPOLLIN) && (ev->events & EPOLLIN)) {
                ev->callback(ev->fd, events[i].events, ev->arg);
            }
            if ((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT)) {
                ev->callback(ev->fd, events[i].events, ev->arg);
            }
        }
    }
}
int main(int argc, char const *argv[]) {
    unsigned short port = SERVER_PORT;
    if (argc == 2) {
        port = atoi(argv[1]);
    }

    // 初始化 reactor
    struct reactor_t *reactor =
        (struct reactor_t *)malloc(sizeof(struct reactor_t));
    reactor_init(reactor);

    int listenfd[LISTEN_PORT_COUNT] = {0};
    int i = 0;
    // 同时监听 LISTEN_PORT_COUNT 个端口
    for (i = 0; i < LISTEN_PORT_COUNT; i++) {
        listenfd[i] = init_sock(port + i);
        // 添加监听
        // accept_cb 是一个接收器，用于服务端接收连接
        reactor_addlistener(reactor, listenfd[i], accept_cb);
    }

    reactor_run(reactor);

    // 销毁释放内存
    reactor_destroy(reactor);

    for (i = 0; i < LISTEN_PORT_COUNT; i++) {
        close(listenfd[i]);
    }

    return 0;
}