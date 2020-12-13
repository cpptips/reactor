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

    while (1) {  // c10k
        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        socklen_t client_len = sizeof(client_addr);

        // 4. 接收请求
        int clientfd =
            accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (clientfd <= 0) continue;

        // 5. 来一个请求，就启动一个线程进行处理
        pthread_t thread_id;
        int ret = pthread_create(&thread_id, NULL, clt_callback, &clientfd);
        if (ret < 0) {
            perror("pthread_create error");
            exit(-1);
        }
    }

    return 0;
}
