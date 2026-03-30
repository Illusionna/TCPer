#ifndef _APP_H_
#define _APP_H_


#include <signal.h>


#include "../stdc/socket.h"
#include "../stdc/threadpool.h"
#include "../stdc/async_log.h"
#include "../stdc/hashmap.h"
#include "../stdc/md5.h"
#include "../stdc/os.h"


#include "parse.h"
#include "usage.h"


#define DEFAULT_SERVICE_PORT 31415
#define BUFFER_CAPACITY 65536
#define PATH_LENGTH_RESTRICTION 1024
#define PASSWORD_TOKEN "THIS_IS_A_TCP_TOKEN_GENERATED_BY_ILLUSIONNA"
#define HANDSHAKE_OK 0x00
#define HANDSHAKE_ERROR_TOKEN 0x01
#define HANDSHAKE_ERROR_PATH 0x02


#if defined(__OS_UNIX__)
    typedef struct __attribute__((packed)) _TransportProtocolHeader {
        uint64 filesize;
        char md5[33];
        char token[65];
        char filename[PATH_LENGTH_RESTRICTION];
    } _TransportProtocolHeader;
#elif defined(__OS_WINDOWS__)
    #pragma pack(push, 1)
    typedef struct _TransportProtocolHeader {
        uint64 filesize;
        char md5[33];
        char token[65];
        char filename[PATH_LENGTH_RESTRICTION];
    } _TransportProtocolHeader;
    #pragma pack(pop)
#endif


void app_start_tips(char *ipv4, int port);


void app_quit_tips();


void app_interrupt(int semaphore);


bool app_handshake(Socket c, _TransportProtocolHeader *meta);


void app_run_service(int port);


void app_thread_task(void *args);


void app_client_send(char *path, char *host_ipv4, int host_port, char *proxy_ipv4, int proxy_port);


#endif