#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_LENGTH 1024
#define MSG_LENGTH 1200000

struct Pipe {
    int fd_send;
    int fd_recv;
};

void sendSingleMsg(char *message, int len, int fd) {
    int start = 0;
    while (len > 0) {
        int sendLen = send(fd, message + start, len, 0);
        if (sendLen <= 0) {
            perror("send");
            exit(-1);
        }
        len -= sendLen;
        start += sendLen;
    }
}

void *handle_chat(void *data) {
    struct Pipe *pipe = (struct Pipe *)data;
    char message[MSG_LENGTH] = "Message: ";
    char buffer[BUFFER_LENGTH];
    ssize_t len;
    int index = 8;

    // handle chat
    while (1) {
        len = recv(pipe->fd_send, buffer, BUFFER_LENGTH, 0);
        if (len <= 0) {
            break;
        }

        // split and send
        int pos = 0;
        for (int i = 0; i < len; ++i) {
            if (buffer[i] == '\n' || buffer[i] == EOF) {
                int singleMsgLen = i - pos + 1;
                strncpy(message + index, buffer + pos, singleMsgLen);
                int sendMsgLen = index + singleMsgLen;
                sendSingleMsg(message, sendMsgLen, pipe->fd_recv);
                pos = i + 1;
                index = 8;
            }
        }

        // copy left message
        if (pos != len) {
            int length = len - pos;
            strncpy(message + index, buffer + pos, length);
            index += length;
        }
    }
    return NULL;
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
    if (listen(fd, 2)) {
        perror("listen");
        return 1;
    }

    // accept
    int fd1 = accept(fd, NULL, NULL);
    int fd2 = accept(fd, NULL, NULL);
    if (fd1 == -1 || fd2 == -1) {
        perror("accept");
        return 1;
    }

    // create thread
    pthread_t thread1, thread2;
    struct Pipe pipe1;
    struct Pipe pipe2;
    pipe1.fd_send = fd1;
    pipe1.fd_recv = fd2;
    pipe2.fd_send = fd2;
    pipe2.fd_recv = fd1;
    pthread_create(&thread1, NULL, handle_chat, (void *)&pipe1);
    pthread_create(&thread2, NULL, handle_chat, (void *)&pipe2);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    return 0;
}
