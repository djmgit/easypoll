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
#include <sys/stat.h>

#define SERVER_PORT 55555
#define MAX_CONN 1024
#define DEFAULT_RESPONSE_SIZE 512
#define DEFAULT_RESP_PREFIX_SIZE 256
#define MAX_EVENTS 32
#define BUFF_SIZE 1024
#define MAX_LINE 256

typedef struct {
    int r_fd;
} active_client_t;

typedef struct {
    char method[20];
    char resource[100];
} request_t;


int get_file_size(int fd) {
    struct stat st;
    if (fstat(fd, &st) != 0) {
        printf("Failed to get file size");
        return -1;
    }

    return st.st_size;
}

void create_response_prefix_with_content_length(char **buff, int size) {
    char prefix[DEFAULT_RESP_PREFIX_SIZE];
    sprintf(prefix, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\nHi\n", size);
    strcpy(*buff, prefix);
}

void parse_request(char *request, request_t *request_parsed) {
    char *request_first_line = strtok(request, "\r\n");
    char *method = strtok(request_first_line, " ");
    char *resource = strtok(NULL, " ");

    strcpy(request_parsed->method, method);
    strcpy(request_parsed->resource, resource);
}

int open_resource(char *server_root, char *resource) {
    char *resource_path = (char *)malloc(strlen(server_root) + strlen(resource));
    sprintf(resource_path, "%s%s", server_root, resource);
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

void generate_405_response(char **buff) {
    char *response = "HTTP/1.1 405 Method Not Allowed\r\nServer: EasyPoll\r\nContent-Length: 0\r\n\r\n";
    strcpy(*buff, response);
}

void generate_404_response(char **buff) {
    char *response = "HTTP/1.1 404 Not Found\r\nServer: EasyPoll\r\nContent-Length: 0\r\n\r\n";
    strcpy(*buff, response);
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
    request_t request_parsed = (request_t){};
    char resp_prefix = (char *)malloc(DEFAULT_RESP_PREFIX_SIZE);
    char buff[BUFF_SIZE];
    char *resp = (char *)malloc(DEFAULT_RESPONSE_SIZE);
    char *server_root = "./www";
    struct epoll_event events[MAX_EVENTS];
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    active_client_t *clients = (active_client_t *)malloc(sizeof(active_client_t) * MAX_CONN);
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
                        printf("Received all data\n");
                        break;
                    } else {
                        printf("Received data:\n %s\n", buff);
                        memset(&request_parsed, 0, sizeof(request_parsed));
                        parse_request(buff, &request_parsed);
                        if (strcmp(request_parsed.method, "GET") != 0) {
                            memset(resp, 0, sizeof(resp));
                            generate_405_response(&resp);
                            write(events[i].data.fd, resp, strlen(resp));
                        } else {
                            int r_fd = open_resource(server_root, request_parsed.resource);
                            if (r_fd < 0) {
                                memset(resp, 0, sizeof(resp));
                                generate_404_response(&resp);
                                write(events[i].data.fd, resp, strlen(resp));
                            } else {
                                int f_size = get_file_size(r_fd);
                                if (f_size < 0) {
                                    generate_404_response(&resp);
                                } else {
                                    printf("file size: %d\n", f_size);
                                    memset(resp_prefix, 0, sizeof(resp_prefix));
                                    create_response_prefix_with_content_length(&resp_prefix, f_size);
                                    printf("%s\n", resp_prefix);
                                    clients[events[i].data.fd] = (active_client_t) {    
                                        .r_fd = r_fd    
                                    };
                                }
                            }
                        }
                    }
                }
            }
            if ((events[i].events & EPOLLOUT) && (events[i].data.fd != server_sock)) {
                if (clients[events[i].data.fd].r_fd >= 0) {
                    char *res = "HTTP/1.1 200 OK\r\nContent-Length: 6\r\nContent-Type: text/html\r\n\r\nHi\n";
                    char *res3 = "Hi\n";
                    write(events[i].data.fd, res, strlen(res));
                    write(events[i].data.fd, res3, strlen(res3));
                    clients[events[i].data.fd].r_fd = -1;
                    printf("Done sending data to fd: %d\n", events[i].data.fd);
                }
            }
            if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
                printf("Connection closed\n\n");
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


