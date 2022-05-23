#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

struct Pipe {
    int fd_send;
    int fd_recv;
};

void sendSingleMsg(char *message, int len, int fd, int flag) {
    // prompt
    char prompt[9] = "Message:";
    if (flag == 1) {
        send(fd, prompt, 8, 0);
    }

    // send message
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

void *handle_chat(void *data) {
    struct Pipe *pipe = (struct Pipe *)data;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE];
    ssize_t len;
    int flag = 1;

    // handle chat
    while (1) {
        len = recv(pipe->fd_send, buffer, BUFFER_SIZE, 0);
        if (len <= 0) {
            break;
        }

        // split and send
        int pos = 0;
        for (int i = 0; i < len; ++i) {
            if (buffer[i] == '\n') {
                int sendMsgLen = i - pos + 1;
                strncpy(message, buffer + pos, sendMsgLen);
                sendSingleMsg(message, sendMsgLen, pipe->fd_recv, flag);
                flag = 1;
                pos = i + 1;
            }
        }

        // send left message
        if (pos != len) {
            int length = len - pos;
            strncpy(message, buffer + pos, length);
            message[length] = '\n';
            sendSingleMsg(message, length + 1, pipe->fd_recv, flag);
            flag = 0;
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
