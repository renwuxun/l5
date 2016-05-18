//
// Created by mofan on 5/17/16.
//

#include "l5.h"


static char* l5_shm_addr = NULL;

static inline char* get_shm_addr() {
    if (NULL == l5_shm_addr) {
//        char f[512]={0};
//        readlink("/proc/self/exe",f, sizeof(f));
//        key_t k = ftok(f, 0);
        int shmid = wx_shm_alloc(0x00002537, BLOCK_START_OFFSET + BLOCK_CNT*sizeof(struct l5_block_s));
        l5_shm_addr = (char*)wx_shm_attach(shmid);
    }
    return l5_shm_addr;
}

void l5_shm_init() {
    const char* addr = get_shm_addr();
    struct wx_spinlock_s* lock = (struct wx_spinlock_s*)(addr + SHM_LOCK_OFFSET);
    wx_spinlock_wlock(lock);

    if (INIT_FLAG != *(uint16_t*)(addr+INIT_FLAG_OFFSET)) {
        *(uint16_t*)(addr+INIT_FLAG_OFFSET) = INIT_FLAG;

        *(uint32_t*)(addr+OFFSET_KEEP_FREE_BLOCK_OFFSET) = BLOCK_START_OFFSET;

        struct l5_block_s* block = (struct l5_block_s*)(addr + BLOCK_START_OFFSET);
        int i;
        for (i=0; i < BLOCK_CNT; i++) {
            block->next = (uint32_t)(((char*)block - addr) + sizeof(struct l5_block_s));
            block++;
        }
        block->next = 0;
    }

    wx_spinlock_wunlock(lock);
}

int l5_shm_detach() {
    return wx_shm_detach(l5_shm_addr);
}

struct l5_block_s* free_block_shift() {
    const char* addr = get_shm_addr();
    const char* addr_keep_free_block_offset = addr+OFFSET_KEEP_FREE_BLOCK_OFFSET;
    uint32_t next_block_offset,free_block_offset;
    for (;;) {
        free_block_offset = *(uint32_t*)addr_keep_free_block_offset;
        next_block_offset = ((struct l5_block_s*)(addr+free_block_offset))->next;
        if (!BLOCK_OFFSET_OK(free_block_offset) || __sync_bool_compare_and_swap((uint32_t*)addr_keep_free_block_offset, free_block_offset, next_block_offset)) {
            break;
        }
    }
    if (!BLOCK_OFFSET_OK(free_block_offset)) {
        wx_err("no more free block");
        return NULL;
    }
    struct l5_block_s* r = (struct l5_block_s*)(addr+free_block_offset);
    r->next = 0;
    return r;
}

void free_block_unshift(struct l5_block_s* block) {
    const char* addr = get_shm_addr();
    const char* addr_keep_free_block_offset = addr+OFFSET_KEEP_FREE_BLOCK_OFFSET;
    uint32_t free_block_offset,new_block_offset = (uint32_t)((char*)block - addr);;
    for (;;) {
        free_block_offset = *(uint32_t*)addr_keep_free_block_offset;
        block->next = free_block_offset;
        if (__sync_bool_compare_and_swap((uint32_t*)addr_keep_free_block_offset, free_block_offset, new_block_offset)) {
            break;
        }
    }
}


int l5_sid_add_ipport(uint16_t sid, uint32_t ip, uint16_t port, uint16_t weight) {
    struct l5_block_s* tmpblock,*lastblock=NULL,*newblock = free_block_shift();
    if (!newblock) {
        return -1;
    }

    uint32_t block_offset;

    const char* addr = get_shm_addr();
    const char* sid_addr = addr+SID_START_OFFSET+sid*(sizeof(uint32_t)*3);
//    const char* sid_counter_addr = sid_addr + sizeof(uint32_t);
    const char* sid_lock_addr = sid_addr + sizeof(uint32_t) + sizeof(uint32_t);

    struct wx_spinlock_s* lock = (struct wx_spinlock_s*)sid_lock_addr;
    wx_spinlock_wlock(lock);

    block_offset = *(uint32_t*)sid_addr;
    for (;BLOCK_OFFSET_OK(block_offset);) {
        tmpblock = (struct l5_block_s*)(addr + block_offset);
        if (tmpblock->ip == ip && tmpblock->port == port) {
            tmpblock->weight = weight;
            free_block_unshift(newblock);
            break;
        }
        lastblock = tmpblock;
        block_offset = tmpblock->next;
    }
    if (!BLOCK_OFFSET_OK(block_offset)) {
        if (lastblock != NULL) {
            lastblock->next = (uint32_t)((char*)newblock-addr);
        } else {
            *(uint32_t*)sid_addr = (uint32_t)((char*)newblock-addr);
        }
        newblock->ip = ip;
        newblock->port = port;
        newblock->weight = weight;
    }
    wx_spinlock_wunlock(lock);

    return 0;
}

void l5_sid_del_ipport(uint16_t sid, uint32_t ip, uint16_t port) {
    uint32_t block_offset, last_offset=0, next_offset=0;
    struct l5_block_s* tmpblock, *found=NULL;

    const char* addr = get_shm_addr();
    const char* sid_addr = addr+SID_START_OFFSET+sid*(sizeof(uint32_t)*3);
//    const char* sid_counter_addr = sid_addr + sizeof(uint32_t);
    const char* sid_lock_addr = sid_addr + sizeof(uint32_t) + sizeof(uint32_t);

    struct wx_spinlock_s* lock = (struct wx_spinlock_s*)sid_lock_addr;
    wx_spinlock_wlock(lock);

    block_offset = *(uint32_t*)sid_addr;
    for (;BLOCK_OFFSET_OK(block_offset);) {
        tmpblock = (struct l5_block_s*)(addr + block_offset);
        if (tmpblock->ip == ip && tmpblock->port == port) {
            next_offset = tmpblock->next;
            found = tmpblock;
            break;
        }
        last_offset = block_offset;
        block_offset = tmpblock->next;
    }
    if (found) {
        if (last_offset == *(uint32_t*)sid_addr) {
            *(uint32_t*)sid_addr = (uint32_t)((char*)found - addr);
        } else {
            ((struct l5_block_s*)(addr+last_offset))->next = next_offset;
        }
        free_block_unshift(found);
    }

    wx_spinlock_wunlock(lock);
}

int l5_sid_route_ipport(uint16_t sid, uint32_t* const ip, uint16_t* const port) {
    int r = -1;
    struct l5_block_s* tmpblock;
    uint32_t block_offset=0, counter=0, _weight_sum,weight_sum=0;

    const char* addr = get_shm_addr();
    const char* sid_addr = addr+SID_START_OFFSET+sid*(sizeof(uint32_t)*3);
    const char* sid_counter_addr = sid_addr + sizeof(uint32_t);
    const char* sid_lock_addr = sid_addr + sizeof(uint32_t) + sizeof(uint32_t);

    struct wx_spinlock_s* lock = (struct wx_spinlock_s*)sid_lock_addr;
    wx_spinlock_rlock(lock);

    counter = *(uint32_t*)sid_counter_addr;

    block_offset = *(uint32_t*)sid_addr;
    for (;BLOCK_OFFSET_OK(block_offset);) {
        tmpblock = (struct l5_block_s*)(addr + block_offset);
        weight_sum += tmpblock->weight;
        block_offset = tmpblock->next;
    }
    block_offset = *(uint32_t*)sid_addr;
    _weight_sum = weight_sum;
    weight_sum = 0;
    for (;BLOCK_OFFSET_OK(block_offset);) {
        tmpblock = (struct l5_block_s*)(addr + block_offset);
        weight_sum += tmpblock->weight;
        if (weight_sum > counter) {
            *ip = tmpblock->ip;
            *port = tmpblock->port;
            r = 0;
            break;
        }
        block_offset = tmpblock->next;
    }

    *(uint32_t*)sid_counter_addr = _weight_sum ? (counter+1)%_weight_sum : counter+1;

    wx_spinlock_runlock(lock);

    return r;
}

void l5_sid_each_block(uint16_t sid, void(*each_cb)(uint32_t ip, uint16_t, uint16_t weight, void* arg), void* arg) {
    struct l5_block_s* tmpblock;
    uint32_t block_offset;

    const char* addr = get_shm_addr();
    const char* sid_addr = addr+SID_START_OFFSET+sid*(sizeof(uint32_t)*3);
//    const char* sid_counter_addr = sid_addr + sizeof(uint32_t);
    const char* sid_lock_addr = sid_addr + sizeof(uint32_t) + sizeof(uint32_t);

    struct wx_spinlock_s* lock = (struct wx_spinlock_s*)sid_lock_addr;
    wx_spinlock_rlock(lock);

    block_offset = *(uint32_t*)sid_addr;
    for (;BLOCK_OFFSET_OK(block_offset);) {
        tmpblock = (struct l5_block_s*)(addr + block_offset);
        each_cb(tmpblock->ip, tmpblock->port, tmpblock->weight, arg);
        block_offset = tmpblock->next;
    }

    wx_spinlock_runlock(lock);
}