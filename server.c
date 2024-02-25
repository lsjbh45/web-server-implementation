#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/epoll.h>

#define BUFFER_SIZE 2048
#define EPOLL_SIZE 100

void get_header(char*, int, long, char*);
void get_content_type(char*, char*);
void handle_error(int, int);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        exit(1);
    }
    
    int port = atoi(argv[1]);
    char* path = argv[2];
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        exit(1);
    }

    struct sockaddr_in host_addr;
    int host_len = sizeof(host_addr);
    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(port);
    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&host_addr, host_len) < 0) {
        exit(1);
    }

    if (listen(server_fd, SOMAXCONN) < 0) {
        exit(1);
    }

    int epoll_fd = epoll_create(EPOLL_SIZE);
    if (epoll_fd < 0) {
        exit(1);
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0) {
        close(server_fd);
        close(epoll_fd);
        exit(1);
    }

    struct epoll_event events[EPOLL_SIZE];

    while (1) {
        int event_count = epoll_wait(epoll_fd, events, EPOLL_SIZE, -1);

        if (event_count < 0) {
            exit(1);
        }

        for (int i = 0; i < event_count; i++) {
            if (events[i].data.fd == server_fd) {
                struct sockaddr_in client_addr;
                int client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&client_len);
 
                if (client_fd < 0) {
                    continue;
                }
 
                struct epoll_event event;
                event.events = EPOLLIN;
                event.data.fd = client_fd;
 
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0) {
                    close(client_fd);
                    continue;
                }
            } else {
                int client_fd = events[i].data.fd;
                
                char header[BUFFER_SIZE];
                char buffer[BUFFER_SIZE];

                int fd_read = read(client_fd, buffer, BUFFER_SIZE);

                if (fd_read == 0) {
                    close(client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    continue;
                }

                if (fd_read < 0) {
                    handle_error(client_fd, 500);
                    continue;
                }

                char *method = strtok(buffer, " ");
                char *uri = strtok(NULL, " ");
                char *protocol = strtok(NULL, "\r\n");
                if (method == NULL || uri == NULL || protocol == NULL) {
                    handle_error(client_fd, 400);
                    continue;
                }
                if (strcmp(method, "GET") || strcmp(protocol, "HTTP/1.1")) {
                    handle_error(client_fd, 400);
                    continue;
                }

                char target[BUFFER_SIZE];
                sprintf(target, "%s%s", path, uri);
                if (!strcmp(uri, "/")) {
                    sprintf(target, "%s%s", path, "/index.html");
                }

                struct stat file_stat;
                if (stat(target, &file_stat) < 0) {
                    handle_error(client_fd, 404);
                    continue;
                }

                int fd = open(target, O_RDONLY);
                if (fd < 0) {
                    handle_error(client_fd, 500);
                    continue;
                }

                char content_type[40];
                get_content_type(content_type, target);

                long content_len = file_stat.st_size;
                get_header(header, 200, content_len, content_type);
                
                write(client_fd, header, strlen(header));

                int size;
                while ((size = read(fd, buffer, BUFFER_SIZE)) > 0) {
                    write(client_fd, buffer, size);
                }
            }
        }
    }

    return 0;
}


void get_header(char *header, int status_code, long content_len, char *content_type) {
    const char HEADER_FORMAT[] = "HTTP/1.1 %d %s\nContent-Length: %ld\nContent-Type: %s\n\n";

    char status_text[50];
    switch (status_code) {
        case 200:
            strcpy(status_text, "OK"); break;
        case 400:
            strcpy(status_text, "Bad Request"); break;
        case 404:
            strcpy(status_text, "Not Found"); break;
        case 500:
            strcpy(status_text, "Internal Server Error"); break;
    }

    sprintf(header, HEADER_FORMAT, status_code, status_text, content_len, content_type);
}

void get_content_type(char *type, char *uri) {
    char *ext = strrchr(uri, '.');
    if (!strcmp(ext, ".html")) {
        strcpy(type, "text/html");
    } else if (!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg")) {
        strcpy(type, "image/jpeg");
    } else if (!strcmp(ext, ".png")) {
        strcpy(type, "image/png");
    } else if (!strcmp(ext, ".css")) {
        strcpy(type, "text/css");
    } else if (!strcmp(ext, ".js")) {
        strcpy(type, "text/javascript");
    } else {
        strcpy(type, "text/plain");
    }
}

void handle_error(int client_fd, int status_code) {
    char content[50];
    switch (status_code) {
        case 400:
            strcpy(content, "<h1>400 Bad Request</h1>"); break;
        case 404:
            strcpy(content, "<h1>404 Not Found</h1>"); break;
        case 500:
            strcpy(content, "<h1>500 Internal Server Error</h1>"); break;
    }

    char header[BUFFER_SIZE];
    get_header(header, status_code, strlen(content), "text/html");

    write(client_fd, header, strlen(header));
    write(client_fd, content, strlen(content));
}
