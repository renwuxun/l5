//
// Created by renwuxun on 5/13/16.
//

#ifndef UUIDWORKER_CONN_H
#define UUIDWORKER_CONN_H



#include <stdlib.h>
#include "wxworker/wxworker.h"

struct conn_s {
    struct wx_conn_s wx_conn;
    struct wx_timer_s closetimer;
    int keepalivems;
    struct conn_s* next;
    char inuse:1;
    struct wx_buf_s* buf;
    char data[128];
};


int conns_alloc(size_t count);
void conns_free();
struct conn_s* conn_get();
void conn_put(struct conn_s* conn);




#endif //UUIDWORKER_CONN_H
