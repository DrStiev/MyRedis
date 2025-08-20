#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

// need a way to print out formatted messages
static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
}

// need a way to print out formatted error messages
static void die(const char *msg) {
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
    abort();
    
}

// need a way to do some interaction from server to client
static void do_something(int connfd) {
    char rbuf[64] = {};
    // use read - write syscalls for sockets. They are the most generic IO interface
    // also usable for disk files, pipes, etc...
    ssize_t n = read(connfd, rbuf, sizeof(rbuf)-1);
    if (n < 0) {
        msg("read() error");
        return;
    }

    fprintf(stderr, "client says: %s\n", rbuf);

    char wbuf[] = "world";
    write(connfd, wbuf, strlen(wbuf));
}

// core part of server
int main() {
    // Step 1: Obtain a socket handle
    /*
    * -------------------------------------------------
    * | Protocol   | Arguments                        |
    * -------------|-----------------------------------
    * | IPv4 + TCP | socket(AF_INET, SOCK_STREAM, 0)  |
    * | IPv6 + TCP | socket(AF_INET6, SOCK_STREAM, 0) |
    * | IPv4 + UDP | socket(AF_INET, SOCK_DGRAM, 0)   |
    * | IPv6 + UDP | socket(AF_INET6, SOCK_DGRAM, 0)  |
    * -------------|-----------------------------------
    */
    int fd = socket(AF_INET, SOCK_STREAM, 0); 
    if (fd < 0) {
        die("socket()");
    }

    // Step 2: Set socket options
    int val = 1;

    /*
    * 2nd + 3rd arguments specifies which option to set
    * 4th argument is the option value
    */
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // Step 3: Bind to and address
    // struct sockaddr_in holds IPv4:port pair stored as big-endian numbers, converted by htons() and htonl()
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);      // port
    addr.sin_addr.s_addr = htonl(0); // wildcard IP 0.0.0.0
    
    int rv = bind(fd,(const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }

    // Step 4: Listen
    // the socket is created after listen()
    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen()");
    }

    // Step 5: Accept connections
    while (true) {
        struct sockaddr_in client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        
        int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
        if (connfd < 0) {
            continue; // an error occurred
        }
        
        // Step 6: Read & Write
        do_something(connfd);
        close(connfd);
    }

    return 0;

}