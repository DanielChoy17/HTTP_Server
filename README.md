/---------------------------------------------------------------------------------
- Daniel Choy
- 2022 Spring
- README.md
- Explanation of Program as a whole

---------------------------------------------------------------------------------/

# Program Explanation
This program, httpserver.c, is a multi-threaded HTTP server. The server executes "forever" without crashing and processes incoming HTTP requests from the client until the user types CTRL-C on the terminal. The server creates, listens, and accepts connections on a socket that is listening on a port. Furthermore, the socket is used by the server to read bytes from or write bytes to the client by treating the socket as a file descriptor that one reads from or writes to. The server produces a response for each request and in the case where the server runs into an error, the corresponding error response gets produced. This server supports three types of HTTP operations: GET, PUT, and APPEND. The GET method gets the contents of the file specified by the URI. The PUT method puts the contents specified in the message body of the request into the file specified by the URI. In the PUT method, if the file specified by the URI does not exist, the server creates the file and puts the contents specified in the message body of the request into the newly created file. Lastly, the APPEND method appends the contents specified in the message body of the request into the file specified by the URI which should exist already. The server also keeps an audit log where it logs the requests that were made to it in the order in which the server processes them. This server uses a thread-pool design where it has two types of threads, worker threads which process requests and a single dispatcher thread which listens for connections and dispatches them to the workers. 

### Design of the main() function

    1.) Handles the command line arguments passed to the server which include port (the port to listen on), threads (the number of worker threads to use), and logfile (the file in which to store the audit log).

    2.) Checks if the correct number of arguments were sent. If the wrong number of arguments were sent, the program will exit.

    3.) Checks if a valid port number was sent. If a non-valid port number was sent, the program will exit.

    4.) signal(SIGPIPE, SIG_IGN) is a signal handler that ignores the SIGPIPE signal. This handler makes it so that socket failures will result in setting errno to EPIPE rather than throwing a signal.
    
    5.) signal(SIGTERM, sigterm_handler) is a signal handler that calls sigterm_handler() whenever SIGTERM is signaled.
    
    6.) signal(SIGINT, sigterm_handler) is a signal handler that calls sigterm_handler() whenever SIGINT is signaled.

    7.) Creates a socket, binds that socket to the local interface, and then listens for requests on the socket. 

    8.) Creates a new queue to store all the connections (connfds) 

    9.) Creates the number of threads indicated by the user to work on the connections in the queue
        -These threads will have the function thread_manager() as a parameter which calls handle_connection() for each connection that is dequeued from the queue

    10.) Creates an infinite loop that accepts connections and enqueues them into the queue

### The algorithm my handle_connection() function undergoes to process a request and handle it is the following:

    1.) Receieve the request from the client.
    
    2.) Parse the request line from the client and store the method, uri, and version.

    3.) Parse through header fields and save Content-Length only if the method is PUT or APPEND. Save the Request-Id for GET, PUT, and APPEND.

    4.) Depending on what method the client requested, execute the code that handles that method.
        --->For PUT and APPEND, read in the rest of the message body sent by the client if the whole message body wasn't read the first time and store the whole message body in a buffer that is the length of the specified Content-Length sent by the client. After the whole message body has been stored into the buffer, proceed to execute the code that accomplishes the method operation whether it is PUT or APPEND.

    5.) Log the request to the audit log.
    
    6.) Clear the buffer I used to store the request line and reset all my variables that I used to its initial state.

    7.) If the client sends another request, repeat the whole process again. If no request is sent, free the memory used by the buffer.

### Possible Server Status Codes:

200 - When a method is succesful

201 - When a URI's file is created

400 - When a request is ill-formatted 

403 - When the server cannot access the URI’s file

404 - When the URI’s file does not exist

500 - When an unexpected issue prevents processing

501 - When a request includes an unimplemented Method

### This program also has the following error handling:

    1.) In order to catch a 400 status code (When a request is ill-formatted), as I parse through the request line and header fields using strtok(), I send a 400 response and exit out of the handle_connection function as soon as there is an instance where when I call strtok() it returns NULL.

    2.) My program catches a 400 status code (When a request is ill-formatted) when the client specified the method PUT or APPEND but didn't include the Content-Length header field in the correct format or didn't include it at all. If the Content-Length header field is found, my program verifies if it's in the format "key: value" or else it will send a 400 response and exit out of the handle_connection function. If the value of the Content-Length header field is a negative number or a letter, my program sends the 400 response and exits the function as well.

    3.) My program catches a 400 status code (When a request is ill-formatted) when the client specifies the version to be anything other than the string "HTTP/1.1" by using strcmp() on the char* variable I stored the version the client sent and the string "HTTP/1.1".

    4.) My program catches a 400 status code (When a request is ill-formatted) when the client sends a URI that doesn't begin with a '/' or ends with a '/'. I verify this by checking the begining and end of the char* variable where I stored the URI the client sent. 

    5.) My program catches a 501 status code when a request includes an unimplemented method. My program checks that the method specified by the client is either "GET", "PUT", or "APPEND" otherwise it sends a 501 response and exits out of the handle_connection function. My program takes care of this by using strcmp() and comparing the char* variable I stored the method the client sent with "GET", "PUT", or "APPEND" which are all valid methods.

    6.) My program catches a 404 status code when the URI’s file does not exist. I take care of this by checking right after calling open() on the URI if errno == ENOENT because if this statement is true, then I send a 404 response.

    7.) My program cathes a 403 status code when the server cannot access the URI’s file. I take care of this by checking right after calling open() on the URI if errno == EACCES or errno == EISDIR because if any of these two statements are true, then I send a 403 response.

### Program Efficiency:

My program is reasonably efficient as it minimizes the amount of times I have to do a system call such as read(), recv(), write(), or send() and only read/write the amount of bytes I need and not exceeding it. I do this by looping through these system calls until the amount of bytes that I wanted to be read/written is accomplished and by having the parameter of these system calls for how many bytes to read/write be only the remaining bytes that are left to read/write. 

Additionally, my program is efficient as it uses the number of threads indicated by the user to constantly handle requests concurrently.

## Table of Contents 
README.md - Explanation of Program as a whole

Makefile - Tool containing a set of commands to make the compilation process easier

httpserver.c - Implementation file for httpserver.c

queue.c - Implementation file for Queue ADT

queue.h - Header file for Queue ADT

## Makefile Directions (Building)
make - makes httpserver

make all - makes httpserver

make httpserver - makes httpserver

make clean - removes all compiler generated files

make format - formats all source and header files

## Running
* Run server on one terminal and send requests to server on another terminal 

### To run the executable of httpserver.c (starting server)
./httpserver [-t threads] [-l logfile] [port number]

### To send the server a request
#### General Format:

    Request-Line\r\n 
    
    (Header-Field\r\n)*
    
    \r\n 
    
    Message-Body

#### GET Method Request:
* Request using printf:
    
    printf "GET /[name of file to GET content from] HTTP/1.1\r\nRequest-Id: [id number]\r\n\r\n" | nc localhost [port number]

* Request using netcat:
    
    nc -C localhost [port number]

    GET /[name of file to GET content from] HTTP/1.1

    Request-Id: [id number]

    >

* Request using curl:
    
    curl -X GET -H "Request-Id: [id number]" localhost:[port number]/[name of file to GET content from]

#### PUT Method Request:
* Request using printf:
    
    printf "PUT /[name of file to PUT content in] HTTP/1.1\r\nContent-Length: [length of content]\r\nRequest-Id: [id number]\r\n\r\n[content you want to PUT]" | nc localhost [port number]

* Request using netcat:

    nc -C localhost [port number]

    PUT /[name of file to PUT content in] HTTP/1.1

    Content-Length: [length of content]

    Request-Id: [id number]
    
    >
    
    [content you want to PUT]

* Request using curl:
    
    curl -X PUT -H "Content-Length: [length of content]" -H "Request-Id: [id number]" -d "[content you want to PUT]" localhost:[port number]/[name of file to PUT content in]

#### APPEND Method Request:
* Request using printf:
    
    printf "APPEND /[name of file to APPEND content in] HTTP/1.1\r\nContent-Length: [length of content]\r\nRequest-Id: [id number]\r\n\r\n[content you want to APPEND]" | nc localhost [port number]

* Request using netcat:
    
    nc -C localhost [port number]
    
    APPEND /[name of file to APPEND content in] HTTP/1.1
    
    Content-Length: [length of content]

    Request-Id: [id number]
    
    >
    
    [content you want to APPEND]

* Request using curl:
    
    curl -X APPEND -H "Content-Length: [length of content]" -H "Request-Id: [id number]" -d "[content you want to APPEND]" localhost:[port number]/[name of file to APPEND content in]
    