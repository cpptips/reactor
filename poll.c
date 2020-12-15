#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
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
    struct pollfd fds[POLL_SIZE] = {0};
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    int max_fd = 0;
    int i = 0;
    for (i = 1; i < POLL_SIZE; i++) {
        fds[i].fd = -1;
    }

    while (1) {
        int nready = poll(fds, max_fd + 1, 5);
        if (nready < 0) continue;

        if ((fds[0].revents & POLLIN) == POLLIN) {
            struct sockaddr_in client_addr;
            memset(&client_addr, 0, sizeof(client_addr));
            socklen_t client_len = sizeof(client_addr);

            int clientfd =
                accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
            if (clientfd <= 0) continue;

            fds[clientfd].fd = clientfd;
            fds[clientfd].events = POLLIN;
            if (clientfd > max_fd) max_fd = clientfd;
            if (--nready == 0) continue;
        }

        for (i = sockfd + 1; i <= max_fd; i++) {
            if (fds[i].revents & (POLLIN | POLLERR)) {
                char buffer[1024] = {0};
                int ret = recv(i, buffer, sizeof(buffer), 0);
                if (ret < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        perror("read all data");
                    }
                    fds[i].fd = -1;
                } else if (ret == 0) {
                    printf(" disconnect %d \n", i);
                    close(i);
                    fds[i].fd = -1;
                    break;
                } else {
                    printf("Recv:%s, %d Bytes\n", buffer, ret);
                }
                if (--nready == 0) break;
            }
        }
    }
    return 0;
}
