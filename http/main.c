#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<dirent.h>
#include<string.h>
#include"head.h"
#include<stdio.h>
#include<stdlib.h>

extern char acc[10];

int main(int argc, char *argv[]) {
    service ser;

    service_init(argc - 1, argv + 1, &ser);

    createService(&ser);

    handleRequest(&ser);

    service_destroy(&ser);

    return 0;

}




