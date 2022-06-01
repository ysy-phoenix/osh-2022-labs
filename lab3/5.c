#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "liburing.h"

#define MAX_CONNECTIONS 32
#define BACKLOG 512
#define MAX_MESSAGE_LEN 1200000
#define BUFFERS_COUNT MAX_CONNECTIONS

void new_client(int fd);
int find_user(int fd);
int add_prompt(char *buffer, int len, char *prompt);
void add_accept(struct io_uring *ring, int fd, struct sockaddr *client_addr, socklen_t *client_len, unsigned flags);
void add_socket_read(struct io_uring *ring, int fd, unsigned gid, size_t size, unsigned flags);
void add_socket_write(struct io_uring *ring, int fd, __u16 bid, size_t size, unsigned flags);
void add_provide_buf(struct io_uring *ring, __u16 bid, unsigned gid);

enum {
    ACCEPT,
    READ,
    WRITE,
    PROV_BUF,
};

typedef struct conn_info {
    __u32 fd;
    __u16 type;
    __u16 bid;
} conn_info;

char bufs[BUFFERS_COUNT][MAX_MESSAGE_LEN] = {0};
int group_id = 1337;
// char message[MAX_MESSAGE_LEN] = {0};
int flag[MAX_CONNECTIONS] = {0};
int client[MAX_CONNECTIONS] = {0};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Please give a port number: ./io_uring_echo_server [port]\n");
        exit(0);
    }

    // some variables we need
    int portno = strtol(argv[1], NULL, 10);
    struct sockaddr_in serv_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // setup socket
    int sock_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    const int val = 1;
    setsockopt(sock_listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // bind and listen
    if (bind(sock_listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error binding socket...\n");
        exit(1);
    }
    if (listen(sock_listen_fd, BACKLOG) < 0) {
        perror("Error listening on socket...\n");
        exit(1);
    }
    printf("io_uring echo server listening for connections on port: %d\n", portno);

    // initialize io_uring
    struct io_uring_params params;
    struct io_uring ring;
    memset(&params, 0, sizeof(params));

    if (io_uring_queue_init_params(2048, &ring, &params) < 0) {
        perror("io_uring_init_failed...\n");
        exit(1);
    }

    // check if IORING_FEAT_FAST_POLL is supported
    if (!(params.features & IORING_FEAT_FAST_POLL)) {
        printf("IORING_FEAT_FAST_POLL not available in the kernel, quiting...\n");
        exit(0);
    }

    // check if buffer selection is supported
    struct io_uring_probe *probe;
    probe = io_uring_get_probe_ring(&ring);
    if (!probe || !io_uring_opcode_supported(probe, IORING_OP_PROVIDE_BUFFERS)) {
        printf("Buffer select not supported, skipping...\n");
        exit(0);
    }
    free(probe);

    // register buffers for buffer selection
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;

    sqe = io_uring_get_sqe(&ring);
    io_uring_prep_provide_buffers(sqe, bufs, MAX_MESSAGE_LEN, BUFFERS_COUNT, group_id, 0);

    io_uring_submit(&ring);
    io_uring_wait_cqe(&ring, &cqe);
    if (cqe->res < 0) {
        printf("cqe->res = %d\n", cqe->res);
        exit(1);
    }
    io_uring_cqe_seen(&ring, cqe);

    // add first accept SQE to monitor for new incoming connections
    add_accept(&ring, sock_listen_fd, (struct sockaddr *)&client_addr, &client_len, 0);

    // start event loop
    while (1) {
        io_uring_submit_and_wait(&ring, 1);
        struct io_uring_cqe *cqe;
        unsigned head;
        unsigned count = 0;

        // go through all CQEs
        io_uring_for_each_cqe(&ring, head, cqe) {
            ++count;
            struct conn_info conn_i;
            memcpy(&conn_i, &cqe->user_data, sizeof(conn_i));

            int type = conn_i.type;
            if (cqe->res == -ENOBUFS) {
                fprintf(stdout, "bufs in automatic buffer selection empty, this should not happen...\n");
                fflush(stdout);
                exit(1);
            } else if (type == PROV_BUF) {
                if (cqe->res < 0) {
                    printf("cqe->res = %d\n", cqe->res);
                    exit(1);
                }
            } else if (type == ACCEPT) {
                int sock_conn_fd = cqe->res;
                // only read when there is no error, >= 0
                if (sock_conn_fd >= 0) {
                    add_socket_read(&ring, sock_conn_fd, group_id, MAX_MESSAGE_LEN, IOSQE_BUFFER_SELECT);
                }

                // new connected client; read data from socket and re-add accept to monitor for new connections
                add_accept(&ring, sock_listen_fd, (struct sockaddr *)&client_addr, &client_len, 0);
                // add new client
                new_client(sock_conn_fd);
            } else if (type == READ) {
                int bytes_read = cqe->res;
                int bid = cqe->flags >> 16;
                int index = find_user(conn_i.fd);
                if (cqe->res <= 0) {
                    // read failed, re-add the buffer
                    add_provide_buf(&ring, bid, group_id);
                    // connection closed or error
                    close(conn_i.fd);
                    // mark flag
                    flag[index] = 0;
                } else {
                    char prompt[10];
                    sprintf(prompt, "user%02d:", index + 1);
                    bytes_read = add_prompt(bufs[bid], bytes_read, prompt);
                    // bytes have been read into bufs, now add write to socket sqe
                    for (int i = 0; i < MAX_CONNECTIONS; ++i) {
                        if (flag[i] && client[i] != conn_i.fd) {
                            add_socket_write(&ring, client[i], bid, bytes_read, 0);
                        }
                    }
                    // add a new read for the existing connection
                    add_socket_read(&ring, conn_i.fd, group_id, MAX_MESSAGE_LEN, IOSQE_BUFFER_SELECT);
                }
            } else if (type == WRITE) {
                // write has been completed, first re-add the buffer
                add_provide_buf(&ring, conn_i.bid, group_id);
            }
        }

        io_uring_cq_advance(&ring, count);
    }
}

void new_client(int fd) {
    int i;
    for (i = 0; i < MAX_CONNECTIONS; ++i) {
        if (!flag[i]) {
            flag[i] = 1;
            client[i] = fd;
            return;
        }
    }
    if (i == MAX_CONNECTIONS) {
        perror("max user");
        exit(1);
    }
}

int find_user(int fd) {
    for (int i = 0; i < MAX_CONNECTIONS; ++i) {
        if (flag[i] && client[i] == fd) {
            return i;
        }
    }
    return -1;
}

int add_prompt(char *buffer, int len, char *prompt) {
    char message[MAX_MESSAGE_LEN];
    // split
    int pos = 0;
    int start = 0;
    for (int i = 0; i < len; ++i) {
        if (buffer[i] == '\n') {
            strncpy(message + start, prompt, 7);
            start += 7;
            int singleMsgLen = i - pos + 1;
            strncpy(message + start, buffer + pos, singleMsgLen);
            pos = i + 1;
            start += singleMsgLen;
        }
    }
    if (pos != len) {
        strncpy(message + start, prompt, 7);
        start += 7;
        int length = len - pos;
        strncpy(message + start, buffer + pos, length);
        start += length;
    }
    message[start] = '\0';
    strncpy(buffer, message, start + 1);
    return start + 1;
}

void add_accept(struct io_uring *ring, int fd, struct sockaddr *client_addr, socklen_t *client_len, unsigned flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_accept(sqe, fd, client_addr, client_len, 0);
    io_uring_sqe_set_flags(sqe, flags);

    conn_info conn_i = {
        .fd = fd,
        .type = ACCEPT,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

void add_socket_read(struct io_uring *ring, int fd, unsigned gid, size_t message_size, unsigned flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_recv(sqe, fd, NULL, message_size, 0);
    io_uring_sqe_set_flags(sqe, flags);
    sqe->buf_group = gid;

    conn_info conn_i = {
        .fd = fd,
        .type = READ,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

void add_socket_write(struct io_uring *ring, int fd, __u16 bid, size_t message_size, unsigned flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_send(sqe, fd, &bufs[bid], message_size, 0);
    io_uring_sqe_set_flags(sqe, flags);

    conn_info conn_i = {
        .fd = fd,
        .type = WRITE,
        .bid = bid,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}

void add_provide_buf(struct io_uring *ring, __u16 bid, unsigned gid) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    io_uring_prep_provide_buffers(sqe, bufs[bid], MAX_MESSAGE_LEN, 1, gid, bid);

    conn_info conn_i = {
        .fd = 0,
        .type = PROV_BUF,
    };
    memcpy(&sqe->user_data, &conn_i, sizeof(conn_i));
}