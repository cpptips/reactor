#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const static int BUFFER_SIZE = 1024;
void *clt_callback(void *arg) {
    int clientfd = *(int *)arg;

    while (1) {
        char buffer[BUFFER_SIZE];
        bzero(&buffer, BUFFER_SIZE);
        int ret = recv(clientfd, buffer, BUFFER_SIZE, 0);
        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("read all data\n");
                return NULL;
            }
        } else if (ret == 0) {
            printf("disconnect\n");
            return NULL;
        } else {
            printf("Recv:%s, %d Bytes\n", buffer, ret);
        }
    }
}
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
    // 下面是 select的使用
    fd_set rfds;
    fd_set rset;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);

    int max_fd = sockfd;
    int i = 0;
    while (1) {
        rset = rfds;
        int nready = select(max_fd + 1, &rset, NULL, NULL, NULL);
        if (nready < 0) {
            printf("select failed:%d\n", errno);
            continue;
        }

        if (FD_ISSET(sockfd, &rset)) {  // accept
            struct sockaddr_in client_addr;
            memset(&client_addr, 0, sizeof(client_addr));
            socklen_t client_len = sizeof(client_addr);

            int clientfd =
                accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
            if (clientfd < 0) {
                continue;
            }
            if (max_fd == FD_SETSIZE) {
                perror("clientfd --> out range\n");
                break;
            }

            FD_SET(clientfd, &rfds);
            if (clientfd > max_fd) max_fd = clientfd;
            if (--nready == 0) continue;
        }

        for (i = sockfd + 1; i <= max_fd; i++) {
            if (FD_ISSET(i, &rset)) {
                char buffer[1024] = {0};
                int ret = recv(i, buffer, sizeof(buffer), 0);
                if (ret < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        printf("read all data\n");
                    }
                    FD_CLR(i, &rfds);
                    close(i);
                } else if (ret == 0) {
                    printf("disconnect %d\n", i);
                    FD_CLR(i, &rfds);
                    close(i);
                    break;
                } else {
                    printf("Recv:%s, %dBytes\n", buffer, ret);
                }
                if (--nready == 0) break;
            }
        }
    }

    return 0;
}
