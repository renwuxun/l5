#include <netinet/tcp.h>
#include <stdint.h>
//#include <gperftools/profiler.h>
#include "conn.h"
#include "wxworker/env.h"
#include "wxworker/conf.h"
#include "tool.h"
#include "l5.h"
#include "wxworker/ip.h"


struct wx_buf_s* alloc_cb(struct wx_conn_s* wx_conn) {
    struct conn_s* conn = container_of(wx_conn, struct conn_s, wx_conn);
    return conn->buf;
}

void conn_close(struct conn_s* conn) {
    wx_timer_stop(&conn->closetimer);
    wx_conn_close(&conn->wx_conn);
    conn_put(conn);
}

void closetimer_cb(struct wx_timer_s* closetimer) {
    struct conn_s* conn = container_of(closetimer, struct conn_s, closetimer);
    conn_close(conn);
}

void cleanup_cb(struct wx_buf_chain_s* bufchain, ssize_t n, void* arg) {
    struct conn_s* conn = (struct conn_s*)arg;

    conn->buf = (struct wx_buf_s*)conn->data;
    conn->buf->base = conn->buf->data;
    conn->buf->size = sizeof(conn->data) - sizeof(struct wx_buf_s);

    if (conn->keepalivems == 0) {
        conn_close(conn);
    } else {
        wx_timer_start(&conn->closetimer, (size_t)conn->keepalivems, closetimer_cb);
    }
}

int read_cb(struct wx_conn_s* wx_conn, struct wx_buf_s* buf, ssize_t n) {
    struct conn_s* conn = container_of(wx_conn, struct conn_s, wx_conn);

    if (n == 0 && buf->size!=0) {
        conn_close(conn);
        return 0;
    }

    wx_timer_stop(&conn->closetimer);

    char* rnrnpos = strstr(buf->data, "\r\n\r\n");
    if (rnrnpos) {
        char resbody[128]={0};

        uint16_t sid;
        if (1 == sscanf(buf->data, "GET /get/%hu HTTP/", &sid)) {
            uint32_t ip;
            uint16_t port;
            if (0 == l5_sid_route_ipport(sid, &ip, &port)) {
                char sip[16]={0};
                int2ip(ip, sip);
                sprintf(resbody, "%s:%d", sip, port);
            } else {
                sprintf(resbody, "0.0.0.0:0");
            }
        } else {
            sprintf(resbody, "bad request");
        }

        if (strstr(buf->data, "HTTP/1.1\r\n")) {
            conn->keepalivems = 15000;
        }

        conn->buf = NULL;

        struct wx_buf_chain_s* bc = (struct wx_buf_chain_s*)conn->data;
        bc->base = bc->data;
        bc->size = sizeof(conn->data) - sizeof(bc);
        bc->cleanup = cleanup_cb;
        bc->arg = conn;
        bc->next = NULL;


        int vertail = 1;
        char connection[]="keep-alive";
        if (conn->keepalivems == 0) {
            vertail = 0;
            sprintf(connection, "close");
        }
        bc->size = (size_t)sprintf(bc->base, "HTTP/1.%d 200 OK\r\nConnection: %s\r\nContent-Length: %d\r\n\r\n%s", vertail, connection, strlen(resbody), resbody);

        wx_conn_write_start(wx_conn, wx_conn->rwatcher.fd, bc);
    }

    if (buf->size == 0) {
        conn->buf = NULL;
        wx_err("header buffer overflow");
        conn_close(conn);
        return EXIT_FAILURE;
    }

    return 0;
}

void accept_cb(struct wx_worker_s* wk) {
    struct conn_s* conn = conn_get();
    if (conn ==  NULL) {
        wx_err("no more free connction");
        return;
    }

    int cfd = wx_accept(wk->listen_fd, NULL, 0);
    if (cfd < 0) {
        conn_put(conn);
        return;
    }

    int p = fcntl(cfd, F_GETFL);
    if (-1 == p || -1 == fcntl(cfd, F_SETFL, p|O_NONBLOCK)) {
        wx_err("fcntl");
        conn_put(conn);
        return;
    }

    int one = 1;
    setsockopt(cfd, SOL_TCP, TCP_NODELAY, &one, sizeof(one));

    conn->buf = (struct wx_buf_s*)conn->data;
    conn->buf->base = conn->buf->data;
    conn->buf->size = sizeof(conn->data) - sizeof(struct wx_buf_s);
    wx_conn_read_start(&conn->wx_conn, cfd);
}

int main(int argc, char** argv) {

    int worker_id = wx_env_get_worker_id();
    if (worker_id < 0) {
        return run_tool(argc, argv);
    }

//    char _b[64]={0};
//    sprintf(_b, "./profiler-%d.pprof", worker_id);
//    ProfilerStart(_b);

    int listen_fd = wx_env_get_listen_fd();
    if (listen_fd < 0) {
        wx_err("listen_fd < 0");
        return EXIT_FAILURE;
    }

    int worker_count = wx_env_get_worker_count();
    if (worker_count < 0) {
        wx_err("worker_count < 0");
        return EXIT_FAILURE;
    }

    char buf32[32]={0};
    size_t connections = 1024;
    if (0 == wx_conf_get("connection", buf32, sizeof(buf32))) {
        connections = (size_t)atoi(buf32);
    }
    if (0 != conns_alloc(connections)) {
        wx_err("conns_alloc");
        return EXIT_FAILURE;
    }

    l5_shm_init();

    wx_worker_init(listen_fd, accept_cb, alloc_cb, read_cb);
    int r = wx_worker_run();

    l5_shm_detach();

    conns_free();

//    ProfilerStop();

    wx_err("worker stop");

    return r;
}