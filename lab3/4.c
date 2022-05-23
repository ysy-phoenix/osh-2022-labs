#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_USER_NUM 32
#define BUFFER_SIZE 1024
#define MSG_SIZE 1200000

struct Pipe {
    int fd_send;
    int fd_recv;
};
int flag[MAX_USER_NUM] = {0};
int client[MAX_USER_NUM] = {0};
char message[MSG_SIZE] = "";
struct epoll_event events[MAX_USER_NUM];

void sendSingleMsg(char *message, int len, int fd) {
    int start = 0;
    while (len > 0) {
        int sendLen = send(fd, message + start, len, 0);
        if (sendLen <= 0) {
            perror("send");
            exit(1);
        }
        len -= sendLen;
        start += sendLen;
    }
}

void sendtoAll(char *message, int uid, int len) {
    for (int i = 0; i < MAX_USER_NUM; ++i) {
        if (flag[i] && i != uid) {
            sendSingleMsg(message, len, client[i]);
        }
    }
}

void handle_chat(int uid) {
    char buffer[BUFFER_SIZE];
    ssize_t len;
    int start = 7;
    int exit_flag = 1;
    sprintf(message, "user%02d:", uid + 1);

    while (1) {
        len = recv(client[uid], buffer, BUFFER_SIZE, 0);

        // close client
        if (len <= 0) {
            if (exit_flag) {
                flag[uid] = 0;
                close(client[uid]);
                return;
            }
            return;
        }
        exit_flag = 0;

        // split and send
        int pos = 0;
        for (int i = 0; i < len; ++i) {
            if (buffer[i] == '\n') {
                int singleMsgLen = i - pos + 1;
                strncpy(message + start, buffer + pos, singleMsgLen);
                int sendMsgLen = start + singleMsgLen;

                // send message to other client
                sendtoAll(message, uid, sendMsgLen);

                pos = i + 1;
                start = 7;
            }
        }
        if (pos != len) {
            int length = len - pos;
            strncpy(message + start, buffer + pos, length);
            start += length;

            // send left message
            if (len < BUFFER_SIZE) {
                message[start] = '\n';
                sendtoAll(message, uid, start + 1);
                start = 7;
            }
        }
    }
    return;
}

void addfd(int epoll_fd, int socket_fd, int uid) {
    struct epoll_event event;
    event.data.u32 = uid;
    event.data.fd = socket_fd;
    event.events = EPOLLIN;
    event.events |= EPOLLET;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event);
    fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL) | O_NONBLOCK);
}

int main(int argc, char **argv) {
    int port = atoi(argv[1]);
    int fd;

    // create socket
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        return 1;
    }

    // local socket
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(addr);

    // bind
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind");
        return 1;
    }

    // listen
    if (listen(fd, MAX_USER_NUM)) {
        perror("listen");
        return 1;
    }

    int epoll_fd = epoll_create(MAX_USER_NUM);
    if (epoll_fd == -1) {
        perror("epoll create");
        return 1;
    }

    addfd(epoll_fd, fd, 65536);

    while (1) {
        int ret = epoll_wait(epoll_fd, events, MAX_USER_NUM, -1); // epoll_wait
        if (ret < 0) {
            perror("epoll wait");
            return 1;
        }

        for (int i = 0; i < ret; ++i) {
            int socket_fd = events[i].data.fd;
            if (socket_fd == fd) { // new user
                int new_fd = accept(fd, NULL, NULL);
                if (new_fd == -1) {
                    perror("accept");
                    return 1;
                }
                for (int j = 0; j < MAX_USER_NUM; ++j) {
                    if (!flag[j]) {
                        addfd(epoll_fd, new_fd, j);
                        flag[j] = 1;
                        client[j] = new_fd;
                        break;
                    }
                }
            } else if (events[i].events & EPOLLIN) { // recv
                handle_chat(events[i].data.u32);
            }
        }
    }

    return 0;
}
