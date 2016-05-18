//
// Created by mofan on 5/17/16.
//

#include "tool.h"
#include "l5.h"
#include "wxworker/ip.h"


void print_item(uint32_t ip, uint16_t port, uint16_t weight, void* arg) {
    char sip[16]={0};
    int2ip(ip, sip);
    printf("%s %d %d\n", sip, port, weight);;
}

void showusage() {
    printf("usage:\n");
    printf("    -g sid                   peek rows of sid\n");
    printf("    -d sid ip port           remove a row\n");
    printf("    -a sid ip port weight    add a row\n");
}

int run_tool(int argc, char** argv) {

    char args[512]={0};
    int i;
    for (i=1;i<argc;i++) {
        if (i>1) {
            strcat(args, " ");
        }
        strcat(args, argv[i]);
    }


    l5_shm_init();


    uint32_t ip;
    uint16_t sid, port, weight;
    char sip[16]={0};
    if (4 == sscanf(args, "-a %hu %s %hu %hu", &sid, sip, &port, &weight)) {// -a sid ip port weight
        ip2int(sip, &ip);
        l5_sid_add_ipport(sid, ip, port, weight);
        l5_sid_each_block(sid, print_item, NULL);
    } else if(3 == sscanf(args, "-d %hu %s %hu", &sid, sip, &port)) {
        ip2int(sip, &ip);
        l5_sid_del_ipport(sid, ip, port);
        l5_sid_each_block(sid, print_item, NULL);
    } else if(1 == sscanf(args, "-g %hu", &sid)) {
        l5_sid_each_block(sid, print_item, NULL);
    } else {
        showusage();
    }


    l5_shm_detach();

    return EXIT_SUCCESS;
}