#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#define POLL_SIZE 1024

int main(int argc, char const *argv[]) {
    const int port = 9090;
    // 0. 创建socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket error\n");
        return -1;
    }

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    // 2. 绑定 地址 端口
    int ret = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        perror("bind error");
        return 2;
    }

    // 3. 监听
    // backlog为5，意思监听队列中连接最多为5，
    // 如果超过5个，那么其余连接会等待
    ret = listen(sockfd, 5);
    if (ret < 0) {
        perror("listen error");
        return 3;
    }
    // 下面是 poll 的使用
    int epoll_fd = epoll_create(1024);
    struct epoll_event ev;
    struct epoll_event events[1024] = {0};

    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev);

    while (1) {
        int nready = epoll_wait(epoll_fd, events, sizeof(events), -1);
        if (nready == -1) {
            printf("epoll_wait\n");
            break;
        }

        int i = 0;
        for (i = 0; i < nready; i++) {
            if (events[i].data.fd == sockfd) {
                struct sockaddr_in client_addr;
                memset(&client_addr, 0, sizeof(client_addr));
                socklen_t client_len = sizeof(client_addr);

                int clientfd = accept(sockfd, (struct sockaddr *)&client_addr,
                                      &client_len);
                if (clientfd < 0) {
                    continue;
                }

                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = clientfd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clientfd, &ev);
            } else {
                int clientfd = events[i].data.fd;
                char buffer[1024] = {0};
                int ret = recv(clientfd, buffer, sizeof(buffer), 0);
                if (ret < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("read all data\n");
                    }

                    close(clientfd);

                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = clientfd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, clientfd, &ev);
                } else if (ret == 0) {
                    printf(" disconnect %d \n", clientfd);
                    close(clientfd);

                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = clientfd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, clientfd, &ev);
                    break;
                } else {
                    printf("Recv:%s, %d Bytes \n", buffer, ret);
                }
            }
        }
    }
    return 0;
}
