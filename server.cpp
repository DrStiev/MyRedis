// stdlib
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// system
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
// C++
#include <map>
#include <string>
#include <vector>
// proj
#include "buf_operations.h"
#include "messages.h"
#include "parser.h"
#include "types.h"

// make the listening socket non-blocking with fcntl
static void fd_set_nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno) {
        die("fcntl error");
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);
    if (errno) {
        die("fcntl error");
    }
    // fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

// the event loop calls back the application code to do the accept()
static Conn *handle_accept(int fd) {
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd < 0) {
        return NULL;
    }

    uint32_t ip = client_addr.sin_addr.s_addr;
    fprintf(stderr, "New client from %u.%u.%u.%u\n", ip & 255, (ip >> 8) & 255,
            (ip >> 16) & 255, ip >> 24, ntohs(client_addr.sin_port));

    // set the new connection fd to non-blocking mode
    fd_set_nb(connfd);

    // create a 'struct Conn'
    Conn *conn = new Conn();
    conn->fd = connfd;
    conn->want_read = true;  // read the first request
    return conn;
}

// the handling is split into try_one_request(). If there is not enough data, it
// will do nothing until a future loop iteration process 1 request if there is
// enough data
static bool try_one_request(Conn *conn) {
    // Step 3: Try to parse the accumulated buffer
    // Protocol: message header
    if (conn->incoming.size() < 4) {
        return false;  // want read
    }

    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if (len > k_max_msg) {  // protocol error
        msg("too long");
        conn->want_close = true;
        return false;  // want close
    }

    // Protocol: message body
    if (4 + len > conn->incoming.size()) {
        return false;  // want read
    }

    const uint8_t *request = &conn->incoming[4];

    // Step 4: Process the parsed message
    // got one request, do some application logic
    std::vector<std::string> cmd;
    if (parse_req(request, len, cmd) < 0) {
        msg("bad request");
        conn->want_close = true;
        return false;  // error
    }

    size_t header_pos = 0;
    response_begin(conn->outgoing, &header_pos);
    do_request(cmd, conn->outgoing);
    response_end(conn->outgoing, header_pos);

    // Step 5: Remove the message from 'Conn:incoming'
    buf_consume(conn->incoming, 4 + len);
    return true;  // Success
}

// Protocol parser with non-blocking read
/*
 * Simple binary protocol
 * Each message consists of a 4-byte little-endian integer indicating the length
 * of the request and the variable-length payload.
 * +-----+------+-----+------+----------+
 * | len | msg1 | len | msg2 | more ... |
 * +-----+------+-----+------+----------+
 *    4B   ....    4B   ....
 */

static void handle_write(Conn *conn) {
    assert(conn->outgoing.size() > 0);
    ssize_t rv = write(conn->fd, &conn->outgoing[0], conn->outgoing.size());

    if (rv < 0 && errno == EAGAIN) {
        return;  // actually not ready
    }

    if (rv < 0) {
        conn->want_close = true;  // error handling
        return;
    }

    // remove written data from 'outgoing'
    buf_consume(conn->outgoing, (size_t)rv);

    // update the readiness intention
    if (conn->outgoing.size() ==
        0) {  // all data is written. Step 2: Written 1 response
        conn->want_read = true;  // Step 3: Wait for more data
        conn->want_write = false;
    }  // else: want write
}

static void handle_read(Conn *conn) {
    // Step 1: Do a non-blocking read
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    if (rv <= 0 && errno == EAGAIN) {
        return;  // actually not ready
    }
    // handle IO error
    if (rv < 0) {
        msg_errno("read() error");
        conn->want_close = true;
        return;  // want close
    }
    // handle EOF
    if (rv == 0) {
        if (conn->incoming.size() == 0) {
            msg("client closed");
        } else {
            msg("unexpected EOF");
        }
        conn->want_close = true;
        return;  // want close
    }

    // Step 2: Add new data to the 'Conn::incoming' buffer
    buf_append(conn->incoming, buf, (size_t)rv);
    // Step 3: Try to parse the accumulated buffer
    // Step 4: Process the parsed message
    // Step 5: Remove the message from 'Conn::incoming'

    // Add pipelining, parse requests and generate responses
    while (try_one_request(conn)) {
    }  // ASSUMPTION: at most 1 request

    // update the readiness intention
    if (conn->outgoing.size() >
        0) {  // has a response. Step 1: Process 1 request
        conn->want_read = false;
        conn->want_write = true;
        // The socket is likely ready to write in a request-response protocol.
        // try to write without waiting for the next iteration
        return handle_write(conn);  // optimization
    }  // else: want read
}

// core part of server
int main() {
    // Step 1: Obtain a socket handle
    /*
     * +------------+----------------------------------+
     * | Protocol   | Arguments                        |
     * +------------+----------------------------------+
     * | IPv4 + TCP | socket(AF_INET, SOCK_STREAM, 0)  |
     * | IPv6 + TCP | socket(AF_INET6, SOCK_STREAM, 0) |
     * | IPv4 + UDP | socket(AF_INET, SOCK_DGRAM, 0)   |
     * | IPv6 + UDP | socket(AF_INET6, SOCK_DGRAM, 0)  |
     * +------------+----------------------------------+
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
    // struct sockaddr_in holds IPv4:port pair stored as big-endian numbers,
    // converted by htons() and htonl()
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);      // port
    addr.sin_addr.s_addr = htonl(0);  // wildcard IP 0.0.0.0

    int rv = bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }

    // Step 4: Listen
    // the socket is created after listen()
    rv = listen(fd, SOMAXCONN);
    if (rv) {
        die("listen()");
    }

    // a map of all client connections, keyed by fd
    std::vector<Conn *> fd2conn;
    // event loop
    std::vector<struct pollfd> poll_args;

    // Step 5: Accept connections
    while (true) {
        // Step 1: Construct the fd list for 'poll()'
        // prepare the arguments of the poll()
        poll_args.clear();
        // put the listening sockets in the first position
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);
        // the rest are connection sockets
        for (Conn *conn : fd2conn) {
            if (!conn) {
                continue;
            }

            struct pollfd pfd = {conn->fd, POLLERR, 0};
            // poll() flags from the application's intent
            if (conn->want_read) {
                pfd.events |= POLLIN;
            }
            if (conn->want_write) {
                pfd.events |= POLLOUT;
            }
            poll_args.push_back(pfd);
        }
        // Step 2: Call 'poll()'
        // wait for readiness
        // poll is the only blocking syscall in the entire program.
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
        if (rv < 0 && errno == EINTR) {
            continue;  // not an error
        }

        if (rv < 0) {
            die("poll");
        }

        // Step 3: Accept new connections
        // handle the listening socket
        if (poll_args[0].revents) {
            if (Conn *conn = handle_accept(fd)) {
                // put it into the map
                if (fd2conn.size() <= (size_t)conn->fd) {
                    fd2conn.resize(conn->fd + 1);
                }
                fd2conn[conn->fd] = conn;
            }
        }

        // Step 4: Invoke application callbacks
        // handle connection sockets
        for (size_t i = 1; i < poll_args.size(); ++i) {  // note: skip the 1st
            uint32_t ready = poll_args[i].revents;
            if (ready == 0) {
                continue;
            }

            Conn *conn = fd2conn[poll_args[i].fd];
            // read and write logic
            if (ready & POLLIN) {
                assert(conn->want_read);
                handle_read(conn);  // application logic
            }
            if (ready & POLLOUT) {
                assert(conn->want_write);
                handle_write(conn);  // application logic
            }

            // Step 5: Terminate connections
            // close the socket from socket error on application logic
            if ((ready & POLLERR) || conn->want_close) {
                (void)close(conn->fd);
                fd2conn[conn->fd] = NULL;
                delete conn;
            }
        }  // for each connection sockets
    }  // the event loop

    return 0;
}