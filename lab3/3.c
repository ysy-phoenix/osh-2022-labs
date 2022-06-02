#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

    int nfds = fd;
    fd_set clients;

    while (1) {
        FD_ZERO(&clients);
        FD_SET(fd, &clients);
        // add fd
        for (int i = 0; i < MAX_USER_NUM; ++i) {
            if (flag[i]) {
                FD_SET(client[i], &clients);
            }
        }

        if (select(nfds + 1, &clients, NULL, NULL, NULL) > 0) {
            if (FD_ISSET(fd, &clients)) { // new user
                int new_fd = accept(fd, NULL, NULL);
                if (new_fd == -1) {
                    perror("accept");
                    return 1;
                }

                // set non block
                fcntl(new_fd, F_SETFL, fcntl(new_fd, F_GETFL, 0) | O_NONBLOCK);
                if (nfds < new_fd) {
                    nfds = new_fd;
                }

                // add new user
                int i;
                for (i = 0; i < MAX_USER_NUM; ++i) {
                    if (!flag[i]) {
                        flag[i] = 1;
                        client[i] = new_fd;
                        break;
                    }
                }
                if (i == MAX_USER_NUM) {
                    perror("max user");
                }
            } else { // recv or exit
                for (int i = 0; i < MAX_USER_NUM; ++i) {
                    if (flag[i] && FD_ISSET(client[i], &clients)) {
                        handle_chat(i);
                    }
                }
            }
        }
    }

    return 0;
}
