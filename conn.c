//
// Created by renwuxun on 5/13/16.
//

#include "conn.h"


static struct conn_s* free_conns = NULL;
static struct conn_s* _free_conns = NULL;

int conns_alloc(size_t count) {
    if (!_free_conns) {
        _free_conns = free_conns = (struct conn_s*)malloc(sizeof(struct conn_s) * count);
        if (_free_conns == NULL) {
            return -1;
        }

        int i;
        for (i=0; i<count; i++) {
            wx_conn_init(&free_conns[i].wx_conn);
            wx_timer_init(&free_conns[i].closetimer);
            free_conns[i].next = (i+1)<count ? &free_conns[i+1] : NULL;
        }
    }
    return 0;
}

void conns_free() {
    if (_free_conns) {
        free(_free_conns);
        _free_conns = NULL;
    }
}

struct conn_s* conn_get() {
    struct conn_s* tmp = NULL;
    if (free_conns) {
        tmp = free_conns;
        free_conns = free_conns->next;
        tmp->next = NULL;
        tmp->inuse = 1;
        tmp->keepalivems = 0;
    }
    return tmp;
}

void conn_put(struct conn_s* conn) {
    if (conn->inuse & 1) {
        conn->inuse = 0;
        int fd = conn->wx_conn.rwatcher.fd;
        if (fd > 0) {
            close(fd);
        }
        conn->next = free_conns;
        free_conns = conn;
    }
}
