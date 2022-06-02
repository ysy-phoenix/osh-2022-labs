#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024
#define MAX_USER_NUM 32
#define QUEUE_SIZE 64
#define MSG_SIZE 1200000

// user
struct User {
    int fd;
    int uid;
    int head;
};
int flag[MAX_USER_NUM] = {0};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t head_mutex = PTHREAD_MUTEX_INITIALIZER;

// message
struct Message {
    char msg[MSG_SIZE];
    int len;
    int uid;
};
struct Message message_queue[QUEUE_SIZE];
int head = 0;
int tail = 0;

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

void sendtoHost(char *message, int uid, int len) {
    pthread_mutex_lock(&mutex);
    struct Message *sendMsg = &message_queue[tail];
    strncpy(sendMsg->msg, message, len);
    sendMsg->uid = uid;
    sendMsg->len = len;
    tail = (tail + 1) % QUEUE_SIZE;
    pthread_mutex_unlock(&mutex);
}

void *handle_host(void *data) {
    while (1) {
        if (head != tail) {
            pthread_mutex_lock(&mutex);
            head = (head + 1) % QUEUE_SIZE;
            pthread_mutex_unlock(&mutex);
        }
    }
    return NULL;
}

void *sendtoClient(void *data) {
    struct User *user = (struct User *)data;
    while (1) {
        if (!flag[user->uid]) {
            return NULL;
        }
        if (user->head != tail) {
            struct Message *sendMsg = &message_queue[user->head];
            user->head = ((user->head) + 1) % QUEUE_SIZE;
            if (user->uid == sendMsg->uid) {
                continue;
            }
            sendSingleMsg(sendMsg->msg, sendMsg->len, user->fd);
        }
    }
    return NULL;
}

void *handle_chat(void *data) {
    struct User *user = (struct User *)data;
    char buffer[BUFFER_SIZE];
    ssize_t len;
    int start = 7;

    char message[MSG_SIZE] = "";
    sprintf(message, "user%02d:", user->uid + 1);

    // handle chat
    while (1) {
        len = recv(user->fd, buffer, BUFFER_SIZE, 0);

        // close client
        if (len <= 0) {
            flag[user->uid] = 0;
            close(user->fd);
            return NULL;
        }
        // split and send
        int pos = 0;

        for (int i = 0; i < len; ++i) {
            if (buffer[i] == '\n') {
                int singleMsgLen = i - pos + 1;
                strncpy(message + start, buffer + pos, singleMsgLen);
                int sendMsgLen = start + singleMsgLen;

                // send message to host
                sendtoHost(message, user->uid, sendMsgLen);

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
                sendtoHost(message, user->uid, start + 1);
                start = 7;
            }
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
    if (listen(fd, MAX_USER_NUM)) {
        perror("listen");
        return 1;
    }

    pthread_t thread[MAX_USER_NUM];
    struct User user[MAX_USER_NUM];

    // host
    pthread_t host_thread;
    pthread_create(&host_thread, NULL, handle_host, NULL);

    // user
    while (1) {
        int fd_new = accept(fd, NULL, NULL);
        if (fd_new == -1) {
            perror("accept");
            return 1;
        }

        // new user
        int cnt;
        for (cnt = 0; cnt < MAX_USER_NUM; cnt++) {
            if (flag[cnt] == 0) {
                flag[cnt] = 1;

                // new user
                user[cnt].fd = fd_new;
                user[cnt].uid = cnt;
                pthread_mutex_lock(&mutex);
                user[cnt].head = head;
                pthread_mutex_unlock(&mutex);

                pthread_create(&thread[cnt], NULL, handle_chat, (void *)&user[cnt]);
                pthread_create(&thread[cnt], NULL, sendtoClient, (void *)&user[cnt]);
                break;
            }
        }

        // max user
        if (cnt == MAX_USER_NUM) {
            perror("max user");
        }
    }

    return 0;
}