#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define SERVER_PORT 55555
#define MAX_CONN 1024
#define MAX_EVENTS 32
#define BUFF_SIZE 1024
#define MAX_LINE 256

typedef struct {
    int r_fd;
} active_client_t;


int open_resource(char *server_root, char *resource) {
    char resource_path = (char *)malloc(strlen(server_root) + strlen(resource) + 1);
    sprintf(resource_path, "%s/%s", server_root, resource);
    int fd = open(resource_path, O_RDONLY);
    return fd;
}

void add_fd_for_events(int epfd, int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        perror("epoll_ctl failed\n");
        exit(1);
    }
}

void setup_server_socket(struct sockaddr_in *addr) {
    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_ANY;
    addr->sin_port = htons(SERVER_PORT);
}

int set_non_blocking(int sockfd) {
    if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK) == -1) {
        return -1;
    }
    return 0;
}

void run_server() {
    int nfds;
    int conn_sock;
    int sockopt  = 1;
    char buff[BUFF_SIZE];
    char *server_root = "./www";
    struct epoll_event events[MAX_EVENTS];
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    active_client_t *clients = (active_client_t *)malloc(sizeof(client_t) * MAX_CONN);
    for (int c = 0; c < MAX_CONN; c++) {
        clients[c] = (active_client_t) {
            .r_fd = -1,
        };
    }

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&sockopt, sizeof(sockopt)) < 0) {
        perror("Failed callin setsockopt");
    }
    setup_server_socket(&server_addr);

    bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    set_non_blocking(server_sock);
    listen(server_sock, MAX_CONN);

    int epfd = epoll_create(1);
    add_fd_for_events(epfd, server_sock, EPOLLIN | EPOLLOUT | EPOLLET);

    int sock_len = sizeof(client_addr);

    // start event loop
    for (;;) {
        nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_sock) {
                conn_sock = accept(server_sock, (struct sockaddr *)&client_addr, &sock_len);
                inet_ntop(AF_INET, (char *)&(client_addr.sin_addr), buff, sizeof(client_addr));
                printf("Accepted connection froms %s:%d\n", buff, ntohs(client_addr.sin_port));

                set_non_blocking(conn_sock);
                add_fd_for_events(epfd, conn_sock, EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLOUT);
            } else if (events[i].events & EPOLLIN) {
                for (;;) {
                    printf("Recived conn fd %d\n", events[i].data.fd);
                    memset(buff, 0, sizeof(buff));
                    int n = read(events[i].data.fd, buff, sizeof(buff));
                    if (n <= 0) {
                        printf("asdhasjkdsjkadh\n");
                        break;
                    } else {
                        printf("Received data:\n %s\n", buff);
                        clients[events[i].data.fd].conn_fd = events[i].data.fd;
                    }
                }
            }
            if ((events[i].events & EPOLLOUT) && (events[i].data.fd != server_sock)) {
                if (clients[events[i].data.fd].conn_fd != -1) {
                    char *res = "HTTP/1.1 200 OK\r\nContent-Length: 6\r\nContent-Type: text/html\r\n\r\nHi\n";
                    char *res3 = "Hi\n";
                    write(events[i].data.fd, res, strlen(res));
                    write(events[i].data.fd, res3, strlen(res3));
                    clients[events[i].data.fd].conn_fd = -1;
                    printf("Done sending data to fd: %d\n", events[i].data.fd);
                }
            }
            if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
                printf("Connection closed\n");
                epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                close(events[i].data.fd);
                continue;
            }
        }
    }    
}

int main() {
    run_server();
}


