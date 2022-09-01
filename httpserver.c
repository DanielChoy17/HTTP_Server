/*********************************************************************************
* Daniel Choy
* 2022 Spring
* httpserver.c
* Implementation file for httpserver.c
*********************************************************************************/

#include "queue.h"
#include <pthread.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>

#define OPTIONS              "t:l:"
#define DEFAULT_THREAD_COUNT 4
#define BLOCK                4096

static FILE *logfile;
#define LOG(...) fprintf(logfile, __VA_ARGS__);

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t full = PTHREAD_COND_INITIALIZER;

BoundedQueue q;

// Converts a string to an 16 bits unsigned integer.
// Returns 0 if the string is malformed or out of the range.
static size_t strtouint16(char number[]) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }
    return num;
}

// Creates a socket for listening for connections.
// Closes the program and prints an error message on error.
static int create_listen_socket(uint16_t port) {
    struct sockaddr_in addr;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        err(EXIT_FAILURE, "socket error");
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof addr) < 0) {
        err(EXIT_FAILURE, "bind error");
    }
    if (listen(listenfd, 128) < 0) {
        err(EXIT_FAILURE, "listen error");
    }
    return listenfd;
}

static void handle_connection(int connfd) {
    char *buffer = malloc(sizeof(char) * BLOCK);
    char *temp;
    char method_buffer[BLOCK];
    char *request;
    char *method;
    char *uri;
    char *version;
    bool flag = false;
    ssize_t bytes = 0;
    ssize_t current = 1;
    size_t curr_buff_size = BLOCK;
    long length;
    ssize_t bytes_written = 0;
    int fd;
    long request_id = 0;
    ssize_t end_of_headers = 0;
    bool headers_found = false;
    errno = 0;
    char *temp_ptr;

    memset(buffer, 0, curr_buff_size);

    // Receiving request from client
    current = recv(connfd, buffer, BLOCK, 0);
    if (current == 0 || strstr(buffer, "HTTP/1.1") == NULL) {
        strcpy(
            method_buffer, "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
        send(connfd, method_buffer, strlen(method_buffer), 0);
        memset(buffer, 0, curr_buff_size);
        free(buffer);
        return;
    }

    // Parsing request line from client
    while (current > 0) {
        temp = malloc(curr_buff_size * sizeof(char));
        if (current < 4 && bytes >= 2 && sizeof(buffer) >= 4) {
            for (ssize_t i = 0; i < current && !headers_found; i++) {
                if (i + bytes >= 2 && strncmp("\r\n\r\n", &buffer[i + bytes - 2], 4) == 0) {
                    end_of_headers = bytes + i + 3 - 2;
                    headers_found = true;
                    break;
                }
            }
        } else {
            for (ssize_t i = 0; i < current - 3 && !headers_found; i++) {
                if (strncmp("\r\n\r\n", &buffer[i + bytes], 4) == 0) {
                    end_of_headers = bytes + i + 3;
                    headers_found = true;
                    break;
                }
            }
        }
        bytes += current;
        if (headers_found) {
            strncpy(temp, buffer, bytes);
            request = strtok(temp, "\r\n");
            if (request != NULL) {
                method = strtok_r(request, " ", &temp_ptr);
                if (method == NULL) {
                    strcpy(method_buffer,
                        "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
                    send(connfd, method_buffer, strlen(method_buffer), 0);
                    memset(buffer, 0, curr_buff_size);
                    free(temp);
                    free(buffer);
                    return;
                }
                uri = strtok_r(NULL, " ", &temp_ptr);
                if (uri == NULL || uri[0] != '/' || uri[strlen(uri) - 1] == '/') {
                    strcpy(method_buffer,
                        "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
                    send(connfd, method_buffer, strlen(method_buffer), 0);
                    memset(buffer, 0, curr_buff_size);
                    free(temp);
                    free(buffer);
                    return;
                }
                version = strtok_r(NULL, "\r\n", &temp_ptr);
                if (version == NULL || (strcmp(version, "HTTP/1.1")) != 0) {
                    strcpy(method_buffer,
                        "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
                    send(connfd, method_buffer, strlen(method_buffer), 0);
                    memset(buffer, 0, curr_buff_size);
                    free(temp);
                    free(buffer);
                    return;
                }
            } else {
                strcpy(method_buffer,
                    "HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nBad Request\n");
                send(connfd, method_buffer, strlen(method_buffer), 0);
                memset(buffer, 0, curr_buff_size);
                free(temp);
                free(buffer);
                return;
            }

            // Parsing through header fields and saving Content-Length and Request-Id for PUT and APPEND
            if ((strcmp(method, "GET")) != 0 && (strcmp(method, "PUT")) != 0
                && (strcmp(method, "APPEND")) != 0) {
                strcpy(method_buffer,
                    "HTTP/1.1 501 Not Implemented\r\nContent-Length: 16\r\n\r\nNot Implemented\n");
                send(connfd, method_buffer, strlen(method_buffer), 0);
                memset(buffer, 0, curr_buff_size);
                free(temp);
                free(buffer);
                return;
            } else {
                if ((strcmp(method, "PUT")) == 0 || (strcmp(method, "APPEND")) == 0) {
                    char *headers_start = strstr(buffer, "\r\n") + 2;
                    char *next_header = strstr(headers_start, "\r\n");
                    char *curr_header = NULL;
                    size_t len;

                    while (next_header != NULL) {
                        len = next_header - headers_start;
                        curr_header = strndup(headers_start, len);
                        if (strncmp("Content-Length:", curr_header, 15) == 0) {
                            char *val = strstr(curr_header, ":");
                            if (val != NULL) {
                                length = atol(val + 1);
                            }
                        } else if (strncmp("Request-Id:", curr_header, 11) == 0) {
                            char *val = strstr(curr_header, ":");
                            if (val != NULL) {
                                request_id = atol(val + 1);
                            }
                        }

                        free(curr_header);
                        if (len == 0) {
                            break;
                        }
                        headers_start = next_header + 2;
                        next_header = strstr(headers_start, "\r\n");
                    }

                    // Handling PUT method
                    if ((strcmp(method, "PUT")) == 0) {
                        fd = open(uri + 1, O_WRONLY | O_TRUNC);

                        if (fd < 0) {
                            if (errno == EACCES) {
                                strcpy(method_buffer, "HTTP/1.1 403 Forbidden\r\nContent-Length: "
                                                      "10\r\n\r\nForbidden\n");
                                send(connfd, method_buffer, strlen(method_buffer), 0);
                                memset(buffer, 0, curr_buff_size);
                                free(temp);
                                free(buffer);

                                // Logging Request
                                LOG("%s,%s,403,%ld\n", method, uri, request_id);
                                fflush(logfile);

                                return;
                            } else {
                                struct stat fd_stats;
                                bool init = fstat(fd, &fd_stats) == 0;
                                if (errno == EISDIR || (init && S_ISDIR(fd_stats.st_mode))) {
                                    strcpy(method_buffer,
                                        "HTTP/1.1 403 Forbidden\r\nContent-Length: "
                                        "10\r\n\r\nForbidden\n");
                                    send(connfd, method_buffer, strlen(method_buffer), 0);
                                    memset(buffer, 0, curr_buff_size);
                                    free(temp);
                                    free(buffer);

                                    // Logging Request
                                    LOG("%s,%s,403,%ld\n", method, uri, request_id);
                                    fflush(logfile);

                                    return;
                                } else {
                                    fd = open(uri + 1, O_WRONLY | O_CREAT | O_TRUNC);
                                    chmod(uri + 1, 0777);

                                    // Getting message body for a newly created file
                                    char *message = malloc((length + 1) * sizeof(char));
                                    ssize_t min_num_bytes_to_recv = end_of_headers + length + 1;
                                    if (min_num_bytes_to_recv >= (ssize_t) curr_buff_size) {
                                        curr_buff_size = min_num_bytes_to_recv;
                                        buffer = (char *) realloc(buffer, curr_buff_size);
                                    }
                                    while (min_num_bytes_to_recv > bytes) {
                                        current = recv(connfd, buffer + bytes,
                                            min_num_bytes_to_recv - bytes, 0);
                                        bytes = bytes + current;
                                    }
                                    for (int i = 0; i < length; i++) {
                                        message[i] = buffer[end_of_headers + i + 1];
                                    }
                                    message[length] = '\0';

                                    // Putting message body to the newly created file
                                    bytes_written = 0;
                                    current = 1;
                                    while (current > 0) {
                                        current = write(
                                            fd, message + bytes_written, length - bytes_written);
                                        bytes_written = bytes_written + current;
                                    }

                                    free(message);
                                    close(fd);

                                    strcpy(method_buffer, "HTTP/1.1 201 Created\r\nContent-Length: "
                                                          "8\r\n\r\nCreated\n");
                                    send(connfd, method_buffer, strlen(method_buffer), 0);

                                    // Logging Request
                                    LOG("%s,%s,201,%ld\n", method, uri, request_id);
                                    fflush(logfile);
                                }
                            }
                        } else {
                            // Getting message body for a file that exists
                            char *message = malloc((length + 1) * sizeof(char));
                            ssize_t min_num_bytes_to_recv = end_of_headers + length + 1;
                            if (min_num_bytes_to_recv >= (ssize_t) curr_buff_size) {
                                curr_buff_size = min_num_bytes_to_recv;
                                buffer = (char *) realloc(buffer, curr_buff_size);
                            }
                            while (min_num_bytes_to_recv > bytes) {
                                current = recv(
                                    connfd, buffer + bytes, min_num_bytes_to_recv - bytes, 0);
                                bytes = bytes + current;
                            }
                            for (int i = 0; i < length; i++) {
                                message[i] = buffer[end_of_headers + i + 1];
                            }
                            message[length] = '\0';

                            // Putting message body to the existing file
                            bytes_written = 0;
                            current = 1;
                            while (current > 0) {
                                current
                                    = write(fd, message + bytes_written, length - bytes_written);
                                bytes_written = bytes_written + current;
                            }

                            free(message);
                            close(fd);

                            strcpy(
                                method_buffer, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n");
                            send(connfd, method_buffer, strlen(method_buffer), 0);

                            // Logging Request
                            LOG("%s,%s,200,%ld\n", method, uri, request_id);
                            fflush(logfile);
                        }
                    }

                    // Handling APPEND method
                    if ((strcmp(method, "APPEND")) == 0) {
                        fd = open(uri + 1, O_APPEND | O_WRONLY);
                        if (errno == ENOENT) {
                            strcpy(method_buffer, "HTTP/1.1 404 Not Found\r\nContent-Length: "
                                                  "10\r\n\r\nNot Found\n");
                            send(connfd, method_buffer, strlen(method_buffer), 0);
                            memset(buffer, 0, curr_buff_size);
                            free(temp);
                            free(buffer);

                            // Logging Request
                            LOG("%s,%s,404,%ld\n", method, uri, request_id);
                            fflush(logfile);

                            return;
                        } else if (errno == EACCES) {
                            strcpy(method_buffer, "HTTP/1.1 403 Forbidden\r\nContent-Length: "
                                                  "10\r\n\r\nForbidden\n");
                            send(connfd, method_buffer, strlen(method_buffer), 0);
                            memset(buffer, 0, curr_buff_size);
                            free(temp);
                            free(buffer);

                            // Logging Request
                            LOG("%s,%s,403,%ld\n", method, uri, request_id);
                            fflush(logfile);

                            return;
                        } else {
                            struct stat fd_stats;
                            bool init = fstat(fd, &fd_stats) == 0;
                            if (errno == EISDIR || (init && S_ISDIR(fd_stats.st_mode))) {
                                strcpy(method_buffer, "HTTP/1.1 403 Forbidden\r\nContent-Length: "
                                                      "10\r\n\r\nForbidden\n");
                                send(connfd, method_buffer, strlen(method_buffer), 0);
                                memset(buffer, 0, curr_buff_size);
                                free(temp);
                                free(buffer);

                                // Logging Request
                                LOG("%s,%s,403,%ld\n", method, uri, request_id);
                                fflush(logfile);

                                return;
                            } else {
                                // Getting message body
                                char *message = malloc((length + 1) * sizeof(char));
                                ssize_t min_num_bytes_to_recv = end_of_headers + length + 1;
                                if (min_num_bytes_to_recv >= (ssize_t) curr_buff_size) {
                                    curr_buff_size = min_num_bytes_to_recv;
                                    buffer = (char *) realloc(buffer, curr_buff_size);
                                }
                                while (min_num_bytes_to_recv > bytes) {
                                    current = recv(
                                        connfd, buffer + bytes, min_num_bytes_to_recv - bytes, 0);
                                    bytes = bytes + current;
                                }
                                for (int i = 0; i < length; i++) {
                                    message[i] = buffer[end_of_headers + i + 1];
                                }
                                message[length] = '\0';

                                // Appending message body to the file
                                bytes_written = 0;
                                current = 1;
                                while (current > 0) {
                                    current = write(
                                        fd, message + bytes_written, length - bytes_written);
                                    bytes_written = bytes_written + current;
                                }

                                free(message);
                                close(fd);

                                strcpy(method_buffer,
                                    "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n");
                                send(connfd, method_buffer, strlen(method_buffer), 0);

                                // Logging Request
                                LOG("%s,%s,200,%ld\n", method, uri, request_id);
                                fflush(logfile);
                            }
                        }
                    }
                } else {
                    // Parsing through header fields and saving Request-Id for GET
                    char *headers_start = strstr(buffer, "\r\n") + 2;
                    char *next_header = strstr(headers_start, "\r\n");
                    size_t len = next_header - headers_start;
                    char *curr_header = NULL;

                    while (len > 0) {
                        curr_header = strndup(headers_start, len);
                        if (strncmp("Request-Id:", curr_header, 11) == 0) {
                            char *val = strstr(curr_header, ":");
                            if (val != NULL) {
                                request_id = atol(val + 1);
                            }
                        }

                        free(curr_header);
                        if (len == 0) {
                            break;
                        }
                        headers_start = next_header + 2;
                        next_header = strstr(headers_start, "\r\n");
                        len = next_header - headers_start;
                    }

                    // Handling GET method
                    fd = open(uri + 1, O_RDONLY);

                    if (errno == ENOENT) {
                        strcpy(method_buffer,
                            "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n");
                        send(connfd, method_buffer, strlen(method_buffer), 0);

                        // Logging Request
                        LOG("%s,%s,404,%ld\n", method, uri, request_id);
                        fflush(logfile);

                        memset(buffer, 0, curr_buff_size);
                        free(temp);
                        free(buffer);

                        return;
                    } else if (errno == EACCES) {
                        strcpy(method_buffer,
                            "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n\r\nForbidden\n");
                        send(connfd, method_buffer, strlen(method_buffer), 0);

                        // Logging Request
                        LOG("%s,%s,403,%ld\n", method, uri, request_id);
                        fflush(logfile);

                        memset(buffer, 0, curr_buff_size);
                        free(temp);
                        free(buffer);

                        return;
                    } else {
                        struct stat fd_stats;
                        bool init = fstat(fd, &fd_stats) == 0;
                        if (errno == EISDIR || (init && S_ISDIR(fd_stats.st_mode))) {
                            strcpy(method_buffer, "HTTP/1.1 403 Forbidden\r\nContent-Length: "
                                                  "10\r\n\r\nForbidden\n");
                            send(connfd, method_buffer, strlen(method_buffer), 0);

                            // Logging Request
                            LOG("%s,%s,403,%ld\n", method, uri, request_id);
                            fflush(logfile);

                            memset(buffer, 0, curr_buff_size);
                            free(temp);
                            free(buffer);

                            return;
                        } else {
                            sprintf(method_buffer, "HTTP/1.1 200 OK\r\nContent-Length: %lu\r\n\r\n",
                                fd_stats.st_size);
                            send(connfd, method_buffer, strlen(method_buffer), 0);

                            // Getting message body from file and writing it to connfd
                            bytes = read(fd, method_buffer, BLOCK);
                            while (bytes > 0) {
                                bytes_written = 0;
                                current = 0;
                                while (bytes_written < bytes) {
                                    current = send(connfd, method_buffer + bytes_written,
                                        bytes - bytes_written, 0);
                                    bytes_written = bytes_written + current;
                                }

                                bytes = read(fd, method_buffer, BLOCK);
                            }

                            close(fd);

                            // Logging Request
                            LOG("%s,%s,200,%ld\n", method, uri, request_id);
                            fflush(logfile);
                        }
                    }
                }

                // Clearing buffer and resetting variables to initial state
                memset(buffer, 0, curr_buff_size);
                errno = 0;
                request_id = 0;
                bytes = 0;
                flag = false;
                end_of_headers = 0;
                headers_found = false;
            }
        }
        current = recv(connfd, buffer + bytes, curr_buff_size - bytes, 0);
        free(temp);
    }
    free(buffer);
    close(connfd);
}

static void sigterm_handler(int sig) {
    if (sig == SIGTERM) {
        warnx("received SIGTERM");
        free(q.buffer);
        fclose(logfile);
        exit(EXIT_SUCCESS);
    } else if (sig == SIGINT) {
        warnx("received SIGINT");
        free(q.buffer);
        fclose(logfile);
        exit(EXIT_SUCCESS);
    }
}

static void usage(char *exec) {
    fprintf(stderr, "usage: %s [-t threads] [-l logfile] <port>\n", exec);
}

void *thread_manager(void *arg) {
    BoundedQueue *q = (BoundedQueue *) arg;

    int connfd = 0;

    while (1) {
        pthread_mutex_lock(&lock);
        while (empty_queue(q)) {
            pthread_cond_wait(&full, &lock);
        }

        dequeue(q, &connfd);

        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&lock);

        handle_connection(connfd);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int opt = 0;
    int threads = DEFAULT_THREAD_COUNT;
    logfile = stderr;

    while ((opt = getopt(argc, argv, OPTIONS)) != -1) {
        switch (opt) {
        case 't':
            threads = strtol(optarg, NULL, 10);
            if (threads <= 0) {
                errx(EXIT_FAILURE, "bad number of threads");
            }
            break;
        case 'l':
            logfile = fopen(optarg, "w");
            if (!logfile) {
                errx(EXIT_FAILURE, "bad logfile");
            }
            break;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        warnx("wrong number of arguments");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t port = strtouint16(argv[optind]);
    if (port == 0) {
        errx(EXIT_FAILURE, "bad port number: %s", argv[1]);
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT, sigterm_handler);

    int listenfd = create_listen_socket(port);

    q = new_queue(4096);

    for (int i = 0; i < threads; i++) {
        pthread_t p;
        if (pthread_create(&p, NULL, thread_manager, &q) != 0) {
            err(EXIT_FAILURE, "pthread_create() failed");
        }
    }

    for (;;) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            warn("accept error");
            continue;
        }

        pthread_mutex_lock(&lock);
        while (full_queue(&q)) {
            pthread_cond_wait(&empty, &lock);
        }

        enqueue(&q, connfd);

        pthread_cond_signal(&full);
        pthread_mutex_unlock(&lock);
    }

    return EXIT_SUCCESS;
}
