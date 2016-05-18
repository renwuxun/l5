//
// Created by mofan on 5/17/16.
//

#ifndef L5WORKER_L5_H
#define L5WORKER_L5_H


#include <unistd.h>
#include <sys/ipc.h>
#include <stdint.h>
#include <stdlib.h>
#include "wxworker/shm.h"
#include "wxworker/spinlock.h"

#define INIT_FLAG_OFFSET (0)
#define SHM_LOCK_OFFSET (16)
#define OFFSET_KEEP_FREE_BLOCK_OFFSET (32)
#define SID_START_OFFSET (1024)
#define SID_CNT (65536)
#define BLOCKS_PER_SID (5)

#define BLOCK_START_OFFSET (SID_START_OFFSET + SID_CNT*sizeof(uint32_t)*3)
#define BLOCK_CNT (SID_CNT*BLOCKS_PER_SID)

#define INIT_FLAG (0x0001)

#define BLOCK_OFFSET_OK(offset) ((BLOCK_START_OFFSET) <= (offset) && (offset) <= (BLOCK_START_OFFSET + (BLOCK_CNT-1)*sizeof(struct l5_block_s)))


struct l5_block_s {
    uint32_t next;
    uint32_t ip;
    uint16_t port;
    uint16_t weight;
};


void l5_shm_init();

int l5_shm_detach();

int l5_sid_add_ipport(uint16_t sid, uint32_t ip, uint16_t port, uint16_t weight);

void l5_sid_del_ipport(uint16_t sid, uint32_t ip, uint16_t port);

int l5_sid_route_ipport(uint16_t sid, uint32_t* const ip, uint16_t* const port);

void l5_sid_each_block(uint16_t sid, void(*each_cb)(uint32_t ip, uint16_t, uint16_t weight, void* arg), void* arg);



#endif //L5WORKER_L5_H
