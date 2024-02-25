# web-server-implementation

Implementation of Simple Web Server supporting HTTP/1.1-subset and multi-client

Lab assignment of 2023-1R "Computer Network" class

## Goals

Implementing an web server

- with the C programming language and BSD socket APIs

- supports the HTTP/1.1 specification, i.e. persistent TCP connection
  (with considering only the GET method and 200, 400, 404 status codes)

- serves multiple clients using I/O multiplexing with `epoll()` function.

## [Implementation](server.c)

## [Report](report/report.md)
