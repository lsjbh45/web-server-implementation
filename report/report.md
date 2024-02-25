## 1. How to build source code

### build

```sh
$ make
```

implementation source code를 build하기 위한 `Makefile` script가 작성되어 있기 때문에, `make` 명령어만 실행한다면 바로 source code의 build가 가능하다.

`Makefile` script는 내부적으로 shell 명령어를 사용해서 `gcc` complier 기반의 compile이 진행되도록 작성되었다. `server.c` source code가 `server.o` object file로 compile된 뒤, 최종 target인 `server` file로 link된다.

`Makefile` script는 작성의 편의를 위해 macro가 포함되어 구성되어 있으며, `clean` 역시 지원하도록 작성되어 있다. script 작성을 위해 [공식 manual](https://www.gnu.org/software/make/manual/make.html)을 참조했다.

## 2. Implementation details

### Preprocessing statements

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>

#define BUFFER_SIZE 2048
#define EPOLL_SIZE 100
```

- Header file 추가: `#include` 문으로 source code에 필요한 header file들을 명시한다. Preprocessing 과정에서 헤더 파일 전체가 source code file에 추가된다.

  - `<stdio.h>`: Standard Input/Output functions (`sprintf()`, ...)
  - `<stdlib.h>`: General purpose functions including memory management (`atoi()`, ...)
  - `<string.h>`: String handling functions and operations (`strcpy()`, ...)
  - `<unistd.h>`: POSIX operating system API functions (`read()`, ...)
  - `<fcntl.h>`: File control options and functions. (`open()`, ...)
  - `<arpa/inet.h>`: Functions for manipulating IP addresses in network byte order. (`htons()`, ...)
  - `<sys/socket.h>`: Functions for BSD socket APIs. (`socket()`, ...)
  - `<sys/stat.h>`: Functions for retrieving file information. (`stat()`, ...)
  - `<sys/epoll.h>`: Functions for event notification and multiplexing. (`epoll_create()`, ...)

- 매크로 상수 정의: `#define` 문으로 source code 전체에 걸쳐서 필요한 상수를 정의해서 사용한다. Preprocessing 과정에서 source code 전체 범위의 상수가 치환된다.
  - `BUFFER_SIZE`: source code 내부에서 사용하는 버퍼의 크기를 지정한다.
  - `EPOLL_SIZE`: I/O multiplexing 과정에서 동시에 처리할 입/출력의 개수를 지정한다.

### `main()`

```c
int main(int argc, char *argv[]) {
    if (argc != 3) {
        exit(1);
    }

    int port = atoi(argv[1]);
    char* path = argv[2];
    /* ... */
}
```

- command-line argument 처리: 프로그램 실행 시 command를 통해 `main` 함수에 전달되는 argument들을 처리한다.
  - accept하는 argument의 개수는 2개이다.
  - argument 1: 서버가 listening하는 port number
  - argument 2: client가 요청하는 resource의 directory path

```c
    /* ... */
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
    /* ... */
```

- server socket initialization: BSD socket API를 사용해서 client의 요청을 listen할 server socket을 initialize한다.
  - `socket()`, `bind()`, `listen()` system call을 사용해서 server의 socket을 생성하고, address를 assign하며, client의 요청을 확인하는 listener socket으로 설정하게 된다.
  - `4. Linux system call functions description` part에서 BSD socket API를 사용한 implementation에 대해 더 구체적인 내용을 설명할 예정이다.

```c
    /* ... */
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
    /* ... */
```

- I/O multiplexing initialization: `epoll()` 방식을 사용해서 I/O multiplexing을 위한 초기화를 수행한다.
  - `epoll`: `select()`, `poll()`과 유사하게, I/O multiplexing을 지원하기 위한 system call이다. 다른 방식과는 다르게 kernel space에서 file descriptor들을 관리하고, 변경된 file descriptor들의 정보만을 user space로 전달하기 때문에 user space에서 file descriptor에 대한 loop가 필요 없고, event 처리에 대한 동작 속도가 빠르다.
    `epoll` 관련 함수들은 Linux에서만 제공하는 system call이기 때문에 다른 운영체제에 이식하기 어렵다.
  - `epoll_create()`:
    - description: kernel space에 I/O multiplexing을 위한 `epoll` instance를 생성한다.
    - arguments
      - `int size`: Linux 2.6.8부터는 값이 무시되지만, 0보다는 큰 값을 argument로 전달해야 한다.
    - return `int`: `epoll` instance의 생성에 성공했다면, `epoll` instance에 대한 file descriptor를 반환한다. 실패했다면, `-1`을 반환한다.
  - `epoll_ctl()`
    - description: `epoll` instance에 대한 control operation을 수행한다. 탐지 대상이 되는 file descriptor를 `epoll` instance에 등록하거나, 수정하거나, 삭제하기 위해 사용된다.
    - arguments
      - `int epfd`: `epoll` instance에 대한 file descriptor 값이다. 이 file descriptor 값에 의해 참조되는 `epoll` instance에 대해 control operation이 수행된다.
      - `int op`: 어떤 control operation을 수행할 지 지정한다. `EPOLL_CTL_ADD` (등록), `EPOLL_CTL_MOD` (수정), `EPOLL_CTL_DEL` (삭제) 중 하나를 지정할 수 있다.
      - `int fd`: control operation의 수행 대상이 되는 file descriptor 값이다.
      - `struct epoll_event *event`: control operation의 수행 대상이 되는 file descriptor의 탐지 대상 event를 지정한다. `EPOLLIN`을 구조체의 `events`로 설정하면, 수신할 데이터가 존재하는 상황을 event의 대상으로 지정하게 된다. 대상 file descriptor와 event 정보가 구조체에 담겨 argument로 전달된다.
    - return `int`: control operation의 수행에 성공했다면, `0`을 반환한다. 실패했다면, `-1`을 반환한다.
  - `epoll_wait()`
    - description: `epoll` instance에 등록된 file descriptor들의 event들을 탐지한다. `epoll` instance에 등록된 file descriptor들 중에서 event가 발생한 file descriptor들의 정보를 user space로 전달한다.
    - arguments
      - `int epfd`: `epoll` instance에 대한 file descriptor 값이다. 이 file descriptor 값에 의해 참조되는 `epoll` instance에 대해 event 탐지가 수행된다.
      - `struct epoll_event *events`: event가 발생한 file descriptor들을 저장할 구조체 배열이다. event가 발생할 때마다 이 argument에 file descriptor가 계속해서 쌓이게 된다.
      - `int maxevents`: `events` argument에 저장할 수 있는 최대 event 개수이다.
      - `int timeout`: event 탐지를 위한 timeout 값이다. `timeout` 값이 `-1`이면, event가 발생할 때까지 무한정 대기한다. `timeout` 값이 `0`이면, event가 발생하지 않으면 바로 return한다. `timeout` 값이 `0`보다 크면, 해당 값만큼의 시간동안 event가 발생하지 않으면 return한다.
    - return `int`: event가 발생한 file descriptor의 개수를 반환한다. `timeout` 시간동안 event가 발생하지 않았다면 `0`이 반환될 수 있다. 오류가 발생했다면, `-1`을 반환한다.
  - implementation: multiple client의 request를 동시에 처리하기 위해서 `epoll` 방식의 I/O multiplexing의 초기 설정을 진행한다.
    - `epoll` instance를 생성하기 위해 `epoll_create()` 함수를 호출한다.
    - listener 역할의 server socket에서 수신할 데이터가 존재하는 상황을 `epoll_ctl()` 함수를 호출해 `epoll` instance의 탐지 대상으로 등록한다.
    - 앞으로 server socket에 client의 connection 요청이 와서 해당 file descriptor가 변화하게 되면, 이 요청의 존재를 `epoll_wait()` 함수를 통해 탐지할 수 있다.

```c
    /* ... */
    struct epoll_event events[EPOLL_SIZE];

    while (1) {
        int event_count = epoll_wait(epoll_fd, events, EPOLL_SIZE, -1);

        if (event_count < 0) {
            exit(1);
        }

        for (int i = 0; i < event_count; i++) {
            if (events[i].data.fd == server_fd) {
                /* ... */
            } else {
                /* ... */
            }
        }
    }
```

- I/O multiplexing 처리: 계속해서 발생한 event들을 탐지하고, event의 종류에 맞게 처리하도록 구현한다.

  - `while(1)` statement로 process가 종료되기 전까지 계속해서 client request를 처리하도록 한다.
  - `epoll_wait()` 함수를 통해 대상에 해당하는 file descriptor에 대한 event를 탐지한다. `EPOLL_SIZE` 상수 값으로 pending 가능한 최대 event의 개수를, `-1` 값으로 event가 발생할 때까지 무한정 대기하도록 하는 설정을 진행할 수 있다.
  - event가 발생한 file descriptor의 개수만큼 `for` statement를 통해 event 처리를 반복해 진행한다.
  - 앞서 초기 설정에서 등록해 둔 listener 역할의 server socket 뿐 아니라, 실제로 connection이 수립된 client와의 통신을 위한 socket도 `epoll`의 대상으로 추가되었기 때문에 client request에 대한 event도 탐지될 수 있다. 따라서 listener socket에 대한 event가 발생했을 때와, connection socket에 대한 event가 발생했을 때를 구분해 처리해야 한다. `event`의 대상 file descriptor와 server socket의 file descriptor를 비교한 `if-else` statement를 통해 구분해 처리할 수 있다.

```c
            /* ... */
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
            }
            /* ... */
```

- listener socket에 대한 event 처리: client의 connection 요청을 처리하고, connection socket을 `epoll` instance에 등록한다.
  - `accept()` 함수를 통해 listener socket에 대한 client의 connection 요청을 수락한다. 해당 client와의 통신을 위한 새로운 connected socket이 만들어지고, 이에 해당하는 file descriptor가 반환된다.
  - 구현하고자 하는 server는 계속해서 client request를 받아서 처리해주어야 하기 때문에, client와의 통신을 위해 새로 만들어진 socket 역시 `epoll` instance에 등록되어야 한다. server socket을 등록한 것과 유사하게, `epoll_ctl()` 함수를 통해 connection socket을 `epoll` instance에 등록한다.

```c
            /* ... */
            else {
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
```

- connection socket에 대한 event 처리: client의 request를 처리하고, request에 대한 response를 전송하거나 connection을 종료한다.
  - `read()` 함수를 호출해 connect 상태의 socket에서 client로부터 request 메시지를 수신한다. `recv()` 함수에서 `flags` argument의 값이 `0`이라면 `read()` 함수와 동일하기 때문에 implementation에 `read()` 함수를 사용하였다.
  - `read()` 함수의 return value는 수신한 메시지의 크기이다. 만약 client가 connection의 종료를 알리는 메시지를 보냈다면 `read()` 함수의 return value가 0이 된다. 이 경우에는 `close()` 함수를 호출해 connection socket을 닫아주고, connection socket의 file descriptor를 `epoll` instance에서 제거해준 뒤 다음 iteration으로 넘어간다.
  - client의 request 메시지를 여러 방식으로 가공하고 분석해서 request에 대한 response를 준비한다. 이 과정에서 error가 발생한 경우에는 `handle_error()` 함수를 호출해 처리를 진행한 뒤 다음 iteration으로 넘어간다.
    - `read()` 함수의 return value가 0보다 작은 경우는 client request를 receive하는 과정에서 error가 발생한 경우이다. internal server error가 발생한 것이므로, status `500`에 대해 처리하도록 한다.
    - client로부터 request를 정상적으로 읽어들였다면, `strtok()` 함수를 통해 공백 또는 개행 문자로 tokenization한 request의 token을 읽어들여 request의 method, uri, protocol 정보를 확인할 수 있다. 만약 해당하는 정보가 존재하지 않거나 잘못되었다면, server가 알지 못하는 형식의 요청이므로 bad request에 해당하는 오류이고, 따라서 status `400`에 대해 처리하도록 한다.
    - request의 uri와 앞서 가공한 command line argument를 적절히 결합해 client가 실제로 요청한 파일의 경로를 알아낸다. 만약 request의 uri가 `/`인 경우, `index.html` 파일을 요청한 것으로 간주한다. `stat()` 함수를 통해 요청한 파일의 상태 및 정보를 확인할 수 있다. 만약 요청한 파일이 존재하지 않는 경우 not found에 해당하는 오류이므로, status `404`에 대해 처리하도록 한다.
    - 요청한 파일이 존재한다면, `open()` 함수를 호출해 해당 파일을 읽기 전용으로 열고, 해당 파일에 대한 file descriptor를 얻는다. 만약 파일을 열지 못했다면, internal server error가 발생한 것이므로 status `500`에 대해 처리하도록 한다.
  - 위 과정에서 문제가 없이 client request를 가공하고 분석했다면, 정상적인 요청이 발생한 것이므로 http/1.1 response를 client에게 전송해야 한다.
    - `get_header()` 함수를 호출해 response header를 생성한다. 이때, response header의 status는 `200 OK`로, content length는 요청한 파일의 크기로 설정하도록 한다. content type은 요청한 파일의 확장자를 분석하는 `get_content_type()` 함수를 호출해 결정한다.
    - `write()` 함수를 호출해 response header를 client에게 전송한다. 마찬가지로 `send()` 함수에서 `flags` argument의 값이 `0`이라면 `write()` 함수와 동일하기 때문에 implementation에 `write()` 함수를 사용하였다.
    - 요청한 파일을 모두 읽어들일 때까지, `read()` 함수를 호출해 요청한 파일의 내용을 읽어들인 뒤, `write()` 함수를 호출해 해당 내용을 client에게 전송한다.

### `get_header()`

```c
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
```

- `get_header`: response header 생성; status code, content length, content type 정보를 받아서 http/1.1 protocol에 맞는 response header를 생성해 저장하는 custom function이다.
  - status code에 해당하는 status text를 `switch` 문을 통해 결정한다. 처리 가능한 status code는 `200`, `400`, `404`, `500`이다.
  - `sprintf()` 함수를 통해 response header의 format에 맞게 formatted string 형식의 header를 생성하고, `header` argument로 전달받은 buffer에 복사한다.

### `get_content_type()`

```c
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
```

- `get_content_type`: content type 결정; 요청한 파일의 확장자를 분석해 content type을 결정하고 저장하는 custom function이다.
  - `strrchr()` 함수를 통해 uri에서 마지막으로 등장하는 `.` 문자의 위치를 찾는다. 해당 위치를 기준으로 확장자를 추출할 수 있다.
  - `strcmp()` 함수를 통해 확장자를 비교하고, 해당하는 content type을 결정한 뒤, `type` argument로 전달받은 buffer에 복사한다. 만약 요청한 파일의 확장자가 `html`, `jpg`, `jpeg`, `png`, `css`, `js`에 해당하지 않는다면, `text/plain`으로 결정한다.

### `handle_error()`

```c
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
```

- `handle_error`: error 처리; status code에 해당하는 error message를 생성해 client socket에 전송하는 custom function이다.
  - status code에 해당하는 error message를 `switch` 문을 통해 결정한다. 처리 가능한 status code는 `400`, `404`, `500`이다.
  - `get_header()` 함수를 호출해 response header를 생성한다. 이때, content length는 결정된 error message의 길이로, content type은 `text/html`로 사용하도록 한다.
  - `write()` 함수를 호출해 response header와 error message를 client에게 전송한다.

## 3. Execution results

build된 server program이 다음과 같이 실행되었고, `resources` directory에 example resource들이 저장되어 있는 상황에서 여러 scenario에 대해 테스트해 보았다.

```bash
$ ./server 8080 resources
```

### Example Scenario

- (dump 1: The initial web page) Firefox browser에서 주소 표시줄에 http://localhost:8080/(http://127.0.0.1:8080/)를 입력해 HTTP 요청을 web server로 전송했을 때, `index.html`을 fetch하게 된다. web server에서는 browser client와 TCP connection을 맺고, client의 request에 대해 `index.html` file의 내용을 response로 보내게 된다.
- (dump 2: After one second, the image is changed to the larger one) Firefox가 `index.html`을 수신해서 분석하게 되면, 이를 browser에 load하기 위해서 `index.html`에 참조된 resource들인 `script.js`, `gr-small.png`, `gr-large.jpg`에 대해 추가적으로 web server에 요청을 보내게 된다. web server에서는 마찬가지로 client의 request에 대해 요청을 받은 file의 내용들을 response로 보내게 된다. 이 과정에서 TCP connection은 종료되지 않고 HTTP/1.1의 persistent connection을 지원하기 위해 유지된다.

  <center><img src=dump1.png width="580px"></center>
  <center><img src=dump2.png width="580px"></center>

### Other Scenarios

- 존재하지 않는 resource에 대한 요청을 보내는 경우 (`GET http://localhost:8080/notfound HTTP/1.1`): web server에서 `404 Not Found` error를 response로 보내게 된다.
- Web server가 인식할 수 없는 형식의 요청을 보내는 경우 (`UNKNOWN http://localhost:8080/ UNKNOWN`): web server에서 `400 Bad Request` error를 response로 보내게 된다.
- Web server가 읽을 수 없는 resource를 요청하는 경우 (`GET http://localhost:8080/invalid HTTP/1.1`): web server에서 `500 Internal Server Error` error를 response로 보내게 된다.
- 요청을 주고받던 Firefox browser가 종료되는 경우 browser에서는 web server로 길이가 0인 connection 종료 요청을 보내게 되고, web server와 client가 맺고 있던 TCP connection이 종료된다.

## 4. Linux system call functions description

이 절에서는 BSD socket API, I/O multiplexing 등 implementation 과정에서 실제로 사용했거나, 사용한 API의 원형이 되는 몇 가지 종류의 linux system call function에 대해 간략히 정리해 보았다. 일부 부족한 부분은 [Linux 매뉴얼](https://man7.org/linux/man-pages/) 페이지를 참조해 작성하였다.

### `socket()`

- description: communication을 위한 endpoint인 socket을 새로 생성한다.
- arguments
  - `int domain`: communication에 사용할 protocol family를 특정한다. protocol family들은 `<sys/socket.h>`에 정의되어 있으며, IPv4 Internet protocol을 사용해 인터넷 상에서 통신하고자 한다면 `AF_INET`을 인자로 지정한다.
  - `int type`: communication의 semantics를 지시한다. TCP 소켓의 생성에는 `SOCK_STREAM` (stream) 타입을, UDP 소켓의 생성에는 `SOCK_DGRAM` (datagram) type을 지정한다. HTTP/1.1 protocol은 TCP protocol을 사용하기 때문에, `SOCK_STREAM` type을 지정한다.
  - `int protocol`: socket에 특정 protocol을 사용하도록 특정한다. 일반적으로는 특정 protocol family의 socket type에 대해 하나의 protocol만이 존재하며, 이때 `protocol` argument 값은 0으로 특정하게 된다.
- return `int`: socket 생성에 성공한다면, 새로운 socket에 대한 file descriptor 값이 return된다. 실패한다면, `-1`이 return된다.

### `connect()`

- description: 특정 socket에 대한 connection initiate를 요청한다. client에서 server와의 연결을 요청하기 위해 사용되는 함수이다. server에서는 `connect()` 요청이 오면 `accept()`로 통신을 위한 새로운 connection을 수립한다.
- arguments
  - `int sockfd`: client socket의 file descriptor 값이다. 이 file descriptor 값에 의해 참조되는 socket을 connection에 사용한다
  - `const struct sockaddr *addr`: connection 요청을 보낼 server의 address 정보이다. socket이 `SOCK_STREAM` type에 해당한다면, `addr` argument에 의해 특정된 address의 socket과 connection 수립을 시도한다.
  - `socklen_t addrlen`: `addr` 구조체의 변수 크기이다.
- return `int`: connection에 성공한다면, `0`이 return된다. 실패한다면, `-1`이 return된다.

### `bind()`

- description: 특정 socket이 `socket()`으로 생성될 때, 이 socket은 name space에 존재하지만 할당된 address는 존재하지 않는다. `bind()`가 바로 특정 socket에 대한 address를 assign하는 역할을 하는 함수이다.
- arguments
  - `int sockfd`: server socket의 file descriptor 값이다. 이 file descriptor 값에 의해 참조되는 socket에 address를 할당한다.
  - `const struct sockaddr *addr`: server socket에 할당하고자 하는 address 정보이다. 소켓이 존재하는 name space(address family)에 따라 name binding의 규칙이 달라진다. address family가 `AF_INET`이므로, `sockaddr_in` 구조체의 정보를 사용하게 된다.
    - `sin_family`: socket과 마찬가지로 address family를 `AF_INET`로 항상 지정한다.
    - `sin_port`: 요청을 받는 port number 정보를 지정한다. network byte order로 지정해야 하기 때문에, `htons(port)`와 같이 host byte order를 network byte order로 변경해 주어야 한다.
    - `sin_addr.s_addr`: 요청을 받는 IP address 정보를 지정한다. `INADDR_ANY`라는 special address를 사용하는 것은, any address를 binding하기 위해서이다. 마찬가지로 network byte order로 지정해야 하기 때문에, `htonl(INADDR_ANY)`와 같이 little endian order를 network byte order로 변경해 주어야 한다.
  - `socklen_t addrlen`: `addr` 구조체의 변수 크기이다.
- return `int`: address의 assign에 성공한다면, `0`이 return된다. 실패한다면, `-1`이 return된다.

### `listen()`

- description: 특정 socket을 connection을 listen하는 passive socket으로 marking하는 함수이다. server socket이 client의 connection 요청을 기다리도록 설정하는 역할을 한다.
- arguments
  - `int sockfd`: server socket의 file descriptor 값이다. 이 file descriptor 값에 의해 참조되는 socket을 passive socket으로 marking한다.
  - `int backlog`: client socket들의 connection 요청을 pending하고 있는 queue의 최대 크기를 지정한다. 시스템에 설정되어 있는 `SOMAXCONN` 값으로 `backlog` argument의 값을 지정할 수 있다.
- return `int`: socket의 marking에 성공한다면, `0`이 return된다. 실패한다면, `-1`이 return된다.

### `accept()`

- description: listener socket의 queue에서 pending 중인 client의 connection 요청을 추출하고, 이를 수락해서 해당 client와의 통신을 위한 새로운 connected socket을 만들어내서 반환하는 함수이다.
- arguments
  - `int sockfd`: server socket의 file descriptor 값이다. 이 file descriptor 값에 의해 참조되는 socket에서 listen해서 pending 중인 connection 요청을 수락하게 된다. original `sockfd`는 `accept()`의 영향을 받지 않는다.
  - `struct sockaddr *restrict addr`: connection 요청을 수락한 대상 client의 address 정보를 저장하기 위한 구조체이다. `accept()` 함수 호출의 side effect로 `addr` 변수에 client의 address 정보가 저장된다.
  - `socklen_t *restrict addrlen`: `addr` 구조체의 변수 크기이다.
- return `int`: connection 요청을 수락하고, 통신을 위한 새로운 socket을 만들어내는 것에 성공했다면, accepted socket에 대한 file descriptor 값이 return된다. 실패했다면, `-1`이 return된다.

### `send()`

- description: connect 상태의 socket에서 상대 socket으로 메시지를 송신하는 함수이다. `send()` 함수에서 `flags` argument의 값이 `0`이라면, `write()` 함수와 equivalent하다. 실제 implementation에서는 `flags` argument를 설정하지 않아도 되었기 때문에, `send()` 함수 대신 `write()` 함수를 사용하였다.
- arguments
  - `int sockfd`: connect 상태인 socket의 file descriptor 값이다. 이 file descriptor 값에 의해 참조되는 socket을 sender로, 상대 socket을 receiver로 해서 데이터를 송신한다.
  - `const void *buf`: 전송하고자 하는 데이터가 저장되어 있는 주소이다.
  - `size_t len`: 송신하고자 하는 데이터의 크기이다.
  - `int flags`: transmission의 제어를 위해 사용되는 flag들에 대한 bitwise OR 값이다.
- return `int`: 메시지 송신에 성공했다면, 송신한 데이터의 크기가 return된다. 실패했다면, `-1`이 return된다.

### `recv()`

- description: connect 상태의 socket에서 상대 socket으로부터 메시지를 수신하는 함수이다. `recv()` 함수에서 `flags` argument의 값이 `0`이라면, `read()` 함수와 equivalent하다. 실제 implementation에서는 `flags` argument를 설정하지 않아도 되었기 때문에, `recv()` 함수 대신 `read()` 함수를 사용하였다.
- arguments
  - `int sockfd`: connect 상태인 socket의 file descriptor 값이다. 이 file descriptor 값에 의해 참조되는 socket을 receiver로, 상대 socket을 sender로 해서 데이터를 수신한다.
  - `void *buf`: 수신한 데이터를 저장하기 위한 버퍼의 주소이다. `recv()` 함수 호출의 side effect로 `buf` 변수에 수신한 데이터가 저장된다.
- `size_t len`: 수신한 데이터의 크기이다. 메시지의 크기가 너무 클 경우, 메시지의 뒷 부분이 discard된다.
- return `int`: 메시지 수신에 성공했다면, 수신한 데이터의 크기가 return된다. 실패했다면, `-1`이 return된다.

### `close()`

- description: file descriptor를 close하는 함수이다. socket에 대한 file descriptor 값을 argument로 사용한다면, server와 client 간의 양방향 socket connection을 종료한다.
- arguments
  - `int fd`: file descriptor 값이다. 이 file descriptor 값에 의해 참조되는 file description을 close한다.
- return `int`: file descriptor를 close하는데 성공했다면, `0`이 return된다. 실패했다면, `-1`이 return된다.

### `shutdown()`

- description: `close()` 함수가 이후 양방향의 데이터 전송을 모두 종료하는 것과 달리, full-duplex socket connection의 일부 또는 전체를 선택해서 특정 방향의 데이터 전송을 종료할 수 있는 함수이다.
- arguments
  - `int sockfd`: socket의 file descriptor 값이다. 이 file descriptor 값에 의해 참조되는 socket connection의 데이터 전송을 종료한다.
  - `int how`: argument의 값이 `SHUT_RD`라면, socket으로부터의 데이터 수신을 종료한다. `SHUT_WR`라면, socket으로부터의 데이터 송신을 종료한다. `SHUT_RDWR`라면, socket으로부터의 데이터 수신과 송신을 종료한다.
- return `int`: socket connection의 데이터 전송을 종료하는데 성공했다면, `0`이 return된다. 실패했다면, `-1`이 return된다.

### `select()`

- description: 단일 thread에서 synchronous I/O multiplexing을 구현하기 위한 함수이다. read, write, exception에 대한 file descriptor set을 각각 argument로 받아서, 각 file descriptor set에 대해 해당 I/O operation을 수행할 준비가 되었는지를 확인한다.
- arguments
  - `int nfds`: 확인할 file descriptor의 개수이다.
  - `fd_set *restrict readfds`: read operation이 가능한지를 확인하는 대상인 file descriptor 집합이다.
  - `fd_set *restrict writefds`: write operation이 가능한지를 확인하는 대상인 file descriptor 집합이다.
  - `fd_set *restrict exceptfds`: exception condition이 발생했는지를 확인하는 대상인 file descriptor 집합이다.
  - `struct timeval *restrict timeout`: `select()` 함수의 blocking을 제어하기 위한 timeout 값이다. `NULL`이라면, file descriptor가 발견될 때까지 무한정 대기한다. `0`이라면, file descriptor가 발견되지 않으면 즉시 return한다. `0`보다 크다면, file descriptor가 발견될 때까지 `timeout` 값만큼 대기한다.
- return `int`: 준비된 file descriptor가 발견되었다면, 발견된 file descriptor의 개수가 return된다. timeout이 발생했다면, `0`이 return된다. 오류가 발생했다면, `-1`이 return된다.

### `poll()`

- description: `select()` 함수와 유사하게, 지정한 소켓의 변화를 확인하는 방식으로 단일 thread에서 synchronous I/O multiplexing을 구현하기 위한 함수이다. `select()` 함수와 달리, `poll()` 함수는 file descriptor의 개수에 제한이 없다. `select()` 함수가 file descriptor set을 직접 argument로 받는 것과 달리, `pollfd` 구조체를 argument로 받아 관심이 있는 condition에 대해서만 control할 수 있다.
- arguments
  - `struct pollfd *fds`: file descriptor 목록에 대해서 특정 I/O operation이 가능한지를 확인하고, 확인된 event들을 돌려받게 되는 argument인 `pollfd` 구조체 배열이다.
  - `nfds_t nfds`: 확인할 file descriptor의 개수이다.
  - `int timeout`: 구조체 없이 timeout 값을 설정할 수 있는 argument이다.
- return `int`: 준비된 file descriptor가 발견되었다면, 발견된 file descriptor의 개수가 return된다. timeout이 발생했다면, `0`이 return된다. 오류가 발생했다면, `-1`이 return된다.
