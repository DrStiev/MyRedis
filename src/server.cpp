// stdlib
#include <assert.h>
#include <errno.h>
#include <math.h>
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
#include <string>
#include <vector>
// proj
#include "common/common.h"
#include "common/messages.h"
#include "common/types.h"
#include "hashtable/hashtable.h"
#include "sorted_set/zset.h"

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

// append to the back
static void buf_append(Buffer &buf, const uint8_t *data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}

// remove from the front
static void buf_consume(Buffer &buf, size_t n) {
    buf.erase(buf.begin(), buf.begin() + n);
}

// the event loop calls back the application code to do the accept()
static Conn *handle_accept(int fd) {
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd < 0) {
        msg_errno("accept() error");
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

// helper function to deal with array indexes. This makes the code less
// error-prone
static bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out) {
    if (cur + 4 > end) {
        return false;
    }
    memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

// remember *& is a reference to a pointer. References are just pointers with
// different syntax
static bool read_str(const uint8_t *&cur, const uint8_t *end, size_t n,
                     std::string &out) {
    if (cur + n > end) {
        return false;
    }
    out.assign(cur, cur + n);
    cur += n;
    return true;
}

/*
 * A Redis request is a list of strings. Representing a list as a chunk of bytes
 * is the task of (de)serialization. Using the same length-prefixed scheme as
 * the outer message format.
 * +------+-----+------+-----+------+-----+-----+------+
 * | nstr | len | str1 | len | str2 | ... | len | strn |
 * +------+-----+------+-----+------+-----+-----+------+
 *    4B     4B    ...    4B   ...
 */
// Step 1: parse the request command. Length-prefixed data parsing (trivial)
static int32_t parse_req(const uint8_t *data, size_t size,
                         std::vector<std::string> &out) {
    const uint8_t *end = data + size;
    uint32_t nstr = 0;
    if (!read_u32(data, end, nstr)) {
        return -1;
    }
    if (nstr > k_max_args) {
        return -1;  // safety limit
    }

    while (out.size() < nstr) {
        uint32_t len = 0;
        if (!read_u32(data, end, len)) {
            return -1;
        }
        out.push_back(std::string());
        if (!read_str(data, end, len, out.back())) {
            return -1;
        }
    }

    if (data != end) {
        return -1;  // trailing garbage
    }

    return 0;
}

// help functions for the serialization
static void buf_append_u8(Buffer &buf, uint8_t data) {
    buf.push_back(data);
}
static void buf_append_u32(Buffer &buf, uint32_t data) {
    buf_append(buf, (const uint8_t *)&data, 4);
}
static void buf_append_i64(Buffer &buf, int64_t data) {
    buf_append(buf, (const uint8_t *)&data, 8);
}
static void buf_append_dbl(Buffer &buf, double data) {
    buf_append(buf, (const uint8_t *)&data, 8);
}

// function to output serialized data
static void out_nil(Buffer &out) {
    buf_append_u8(out, TAG_NIL);
}

// function to output serialized data
static void out_str(Buffer &out, const char *s, size_t size) {
    buf_append_u8(out, TAG_STR);
    buf_append_u32(out, (uint32_t)size);
    buf_append(out, (const uint8_t *)s, size);
}

// function to output serialized data
static void out_int(Buffer &out, int64_t val) {
    buf_append_u8(out, TAG_INT);
    buf_append_i64(out, val);
}

// function to output serialized data
static void out_dbl(Buffer &out, double val) {
    buf_append_u8(out, TAG_DBL);
    buf_append_dbl(out, val);
}

// function to output serialized data
static void out_arr(Buffer &out, uint32_t n) {
    buf_append_u8(out, TAG_ARR);
    buf_append_u32(out, n);
}

static size_t out_begin_arr(Buffer &out) {
    out.push_back(TAG_ARR);
    buf_append_u32(out, 0);  // filled by out_end_arr()
    return out.size() - 4;   // the 'ctx' arg
}

static void out_end_arr(Buffer &out, size_t ctx, uint32_t n) {
    assert(out[ctx - 1] == TAG_ARR);
    memcpy(&out[ctx], &n, 4);
}

// function to output serialized data
static void out_err(Buffer &out, uint32_t code, const std::string &msg) {
    buf_append_u8(out, TAG_ERR);
    buf_append_u32(out, code);
    buf_append_u32(out, (uint32_t)msg.size());
    buf_append(out, (const uint8_t *)msg.data(), msg.size());
}

static Entry *entry_new(uint32_t type) {
    Entry *ent = new Entry();
    ent->type = type;
    return ent;
}

static void del(Entry *ent) {
    if (ent->type == T_ZSET) {
        clear(&ent->zset);
    }
    delete ent;
}

// equality comparison  for the top-level hashtable
static bool eq(HashNode *node, HashNode *key) {
    struct Entry *ent = container_of(node, struct Entry, node);
    struct LookupKey *keydata = container_of(key, struct LookupKey, node);
    return ent->key == keydata->key;
}

static void do_get(std::vector<std::string> &cmd, Buffer &out) {
    // a dummy 'Entry' just for the lookup
    LookupKey key;
    key.key.swap(cmd[1]);
    key.node.hcode = hash((uint8_t *)key.key.data(), key.key.size());

    // hashtable lookup
    HashNode *node = lookup(&g_data.db, &key.node, &eq);
    if (!node) {
        return out_nil(out);
    }

    // copy the value
    Entry *ent = container_of(node, Entry, node);
    if (ent->type != T_STR) {
        return out_err(out, ERR_BAD_TYP, "not a string value");
    }
    return out_str(out, ent->str.data(), ent->str.size());
}

static void do_set(std::vector<std::string> &cmd, Buffer &out) {
    // dummy 'Entry' just for the lookup
    LookupKey key;
    key.key.swap(cmd[1]);
    key.node.hcode = hash((uint8_t *)key.key.data(), key.key.size());

    // hashtable lookup
    HashNode *node = lookup(&g_data.db, &key.node, &eq);
    if (node) {
        // found, update the value
        Entry *ent = container_of(node, Entry, node);
        if (ent->type != T_STR) {
            return out_err(out, ERR_BAD_TYP, "a non-string value exists");
        }
        ent->str.swap(cmd[2]);
    } else {
        // not found, allocate & insert a new pair
        Entry *ent = entry_new(T_STR);
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        ent->str.swap(cmd[2]);
        insert(&g_data.db, &ent->node);
    }
    return out_nil(out);
}

static void do_del(std::vector<std::string> &cmd, Buffer &out) {
    // dummy 'Entry' just for the lookup
    LookupKey key;
    key.key.swap(cmd[1]);
    key.node.hcode = hash((uint8_t *)key.key.data(), key.key.size());
    //  hashable delete
    HashNode *node = del(&g_data.db, &key.node, &eq);
    if (node) {  // deallocate the pair
        del(container_of(node, Entry, node));
    }
    return out_int(out, node ? 1 : 0);
}

static bool cb_keys(HashNode *node, void *arg) {
    Buffer &out = *(Buffer *)arg;
    const std::string &key = container_of(node, Entry, node)->key;
    out_str(out, key.data(), key.size());
    return true;
}

static void do_keys(std::vector<std::string> &, Buffer &out) {
    out_arr(out, (uint32_t)size(&g_data.db));
    foreach (&g_data.db, &cb_keys, (void *)&out)
        ;
}

static bool str2dbl(const std::string &s, double &out) {
    char *endp = NULL;
    out = strtod(s.c_str(), &endp);
    return endp == s.c_str() + s.size() && !isnan(out);
}

static bool str2int(const std::string &s, int64_t &out) {
    char *endp = NULL;
    out = strtoll(s.c_str(), &endp, 10);
    return endp == s.c_str() + s.size();
}

static void do_zadd(std::vector<std::string> &cmd, Buffer &out) {
    double score = 0;
    if (!str2dbl(cmd[2], score)) {
        return out_err(out, ERR_BAD_ARG, "expect float");
    }

    // look up or create zset
    LookupKey key;
    key.key.swap(cmd[1]);
    key.node.hcode = hash((uint8_t *)key.key.data(), key.key.size());
    HashNode *hnode = lookup(&g_data.db, &key.node, &eq);

    Entry *ent = NULL;
    if (!hnode) {  // insert a new key
        ent = entry_new(T_ZSET);
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        insert(&g_data.db, &ent->node);
    } else {  // check the existing key
        ent = container_of(hnode, Entry, node);
        if (ent->type != T_ZSET) {
            return out_err(out, ERR_BAD_TYP, "expect zset");
        }
    }

    // add or update the tuple
    const std::string &name = cmd[3];
    bool added = insert(&ent->zset, name.data(), name.size(), score);
    return out_int(out, (int64_t)added);
}

static ZSet *expect_zset(std::string &s) {
    LookupKey key;
    key.key.swap(s);
    key.node.hcode = hash((uint8_t *)key.key.data(), key.key.size());
    HashNode *hnode = lookup(&g_data.db, &key.node, &eq);
    if (!hnode) {  // non-existent key is treated as an empty zset
        return (ZSet *)&k_empty_zset;
    }
    Entry *ent = container_of(hnode, Entry, node);
    return ent->type == T_ZSET ? &ent->zset : NULL;
}

static void do_zrem(std::vector<std::string> &cmd, Buffer &out) {
    ZSet *zset = expect_zset(cmd[1]);
    if (!zset) {
        return out_err(out, ERR_BAD_TYP, "expect zset");
    }

    const std::string &name = cmd[2];
    ZNode *znode = lookup(zset, name.data(), name.size());
    if (znode) {
        del(zset, znode);
    }
    return out_int(out, znode ? 1 : 0);
}

static void do_zscore(std::vector<std::string> &cmd, Buffer &out) {
    ZSet *zset = expect_zset(cmd[1]);
    if (!zset) {
        return out_err(out, ERR_BAD_TYP, "expected zset");
    }

    const std::string &name = cmd[2];
    ZNode *znode = lookup(zset, name.data(), name.size());
    return znode ? out_dbl(out, znode->score) : out_nil(out);
}

static void do_zquery(std::vector<std::string> &cmd, Buffer &out) {
    // parse args
    double score = 0;
    if (!str2dbl(cmd[2], score)) {
        return out_err(out, ERR_BAD_ARG, "expect floating point number");
    }
    const std::string &name = cmd[3];
    int64_t _offset = 0, limit = 0;
    if (!str2int(cmd[4], _offset) || !str2int(cmd[5], limit)) {
        return out_err(out, ERR_BAD_ARG, "expect int");
    }

    // get the zset
    ZSet *zset = expect_zset(cmd[1]);
    if (!zset) {
        return out_err(out, ERR_BAD_TYP, "expect zset");
    }

    // seek key
    if (limit <= 0) {
        return out_arr(out, 0);
    }
    ZNode *znode = seekge(zset, score, name.data(), name.size());
    znode = offset(znode, _offset);

    // output
    size_t ctx = out_begin_arr(out);
    int64_t n = 0;
    while (znode && n < limit) {
        out_str(out, znode->name, znode->len);
        out_dbl(out, znode->score);
        znode = offset(znode, +1);
        n += 2;
    }
    out_end_arr(out, ctx, (uint32_t)n);
}

// Step 2: Process the command
static void do_request(std::vector<std::string> &cmd, Buffer &out) {
    if (cmd.size() == 2 && cmd[0] == "get") {
        return do_get(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "set") {
        return do_set(cmd, out);
    } else if (cmd.size() == 2 && cmd[0] == "del") {
        return do_del(cmd, out);
    } else if (cmd.size() == 1 && cmd[0] == "keys") {
        return do_keys(cmd, out);
    } else if (cmd.size() == 4 && cmd[0] == "zadd") {
        return do_zadd(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "zrem") {
        return do_zrem(cmd, out);
    } else if (cmd.size() == 3 && cmd[0] == "zscore") {
        return do_zscore(cmd, out);
    } else if (cmd.size() == 6 && cmd[0] == "zquery") {
        return do_zquery(cmd, out);
    } else {
        return out_err(out, ERR_UNKNOWN, "unknown command");
    }
}

// Step 3: Serialize the response
static void response_begin(Buffer &out, size_t *header) {
    *header = out.size();    // message header position
    buf_append_u32(out, 0);  // reserve space
}

static size_t response_size(Buffer &out, size_t header) {
    return out.size() - header - 4;
}

static void response_end(Buffer &out, size_t header) {
    size_t msg_size = response_size(out, header);
    if (msg_size > k_max_msg) {
        out.resize(header + 4);
        out_err(out, ERR_TOO_BIG, "response is too big");
        msg_size = response_size(out, header);
    }
    // message header
    uint32_t len = (uint32_t)msg_size;
    memcpy(&out[header], &len, 4);
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
        msg_errno("write() error");
        conn->want_close = true;  // error handling
        return;
    }

    // remove written data from 'outgoing'
    buf_consume(conn->outgoing, (size_t)rv);

    // update the readiness intention
    if (conn->outgoing.size() == 0) {  // all data is written
                                       // Step 2: Written 1 response
        conn->want_read = true;        // Step 3: Wait for more data
        conn->want_write = false;
    }  // else: want write
}

static void handle_read(Conn *conn) {
    // Step 1: Do a non-blocking read
    uint8_t buf[64 * 1024];
    ssize_t rv = read(conn->fd, buf, sizeof(buf));
    if (rv < 0 && errno == EAGAIN) {
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
    if (conn->outgoing.size() > 0) {  // has a response
                                      // Step 1: Process 1 request
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
    addr.sin_port = ntohs(1234);      // port
    addr.sin_addr.s_addr = ntohl(0);  // wildcard IP 0.0.0.0

    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    if (rv) {
        die("bind()");
    }

    // set the listen fd to nonblocking mode
    fd_set_nb(fd);

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

            // always poll() for error
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
                assert(!fd2conn[conn->fd]);
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
