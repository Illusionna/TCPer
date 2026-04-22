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


enum {
    APP_COMMAND_SEND = 0x01,
    APP_COMMAND_LIST = 0x02,
    APP_COMMAND_DOWNLOAD = 0x03,
    APP_COMMAND_REMOVE = 0x04,
    HANDSHAKE_OK = 0x05,
    HANDSHAKE_ERROR_TOKEN = 0x06,
    HANDSHAKE_ERROR_PATH = 0x07
};


#define DEFAULT_SERVICE_PORT 31415
#define BUFFER_CAPACITY 65536
#define PATH_LENGTH_RESTRICTION 1024
#define PASSWORD_TOKEN "THIS_IS_A_TCP_TOKEN_GENERATED_BY_ILLUSIONNA"    // You can customize the password toekn.


#if defined(__OS_UNIX__)
    typedef struct __attribute__((packed)) _TransportProtocolHeader {
        byte command;
        uint64 filesize;
        char md5[33];
        char token[65];
        char filename[PATH_LENGTH_RESTRICTION];
    } _TransportProtocolHeader;
#elif defined(__OS_WINDOWS__)
    #pragma pack(push, 1)
    typedef struct _TransportProtocolHeader {
        byte command;
        uint64 filesize;
        char md5[33];
        char token[65];
        char filename[PATH_LENGTH_RESTRICTION];
    } _TransportProtocolHeader;
    #pragma pack(pop)
#endif


static inline void app_start_tips(char *ipv4, int port) {
    printf(" * \x1b[1;32mINFO:\x1b[0m started server process ID [\x1b[1;36m%d\x1b[0m]\n", os_getpid());
    printf(" * \x1b[1;32mINFO:\x1b[0m waiting for application startup.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m application startup complete.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m C socket TCP service is running on \x1b[1;36m%s:%d\x1b[0m (press \x1b[1;33mCTRL+C\x1b[0m to quit)\n", ipv4, port);
}


static inline void app_quit_tips() {
    printf(" * \x1b[1;32mINFO:\x1b[0m shutting down.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m waiting for application shutdown.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m application shutdown complete.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m finished server process ID [\x1b[1;36m%d\x1b[0m]\n", os_getpid());
}


void app_interrupt(int semaphore);


bool app_handshake(Socket c, _TransportProtocolHeader *meta);


void app_run_service(int port);


void app_thread_task(void *args);


void app_server_send(Socket c, _TransportProtocolHeader *meta);


void app_server_list(Socket c, _TransportProtocolHeader *meta);


void app_server_download(Socket c, _TransportProtocolHeader *meta);


void app_server_remove(Socket c, _TransportProtocolHeader *meta);


void app_server_default(Socket c);


void app_client_send(char *path, char *host_ipv4, int host_port, char *proxy_ipv4, int proxy_port);


void app_client_list(char *dir, char *host_ipv4, int host_port);


void app_client_download(char *remote_file, char *save_path, char *host_ipv4, int host_port, char *proxy_ipv4, int proxy_port);


void app_client_remove(char *path, char *host_ipv4, int host_port);


void __app_server_list__(char *dir, char *name, bool folder, uint64 size, void *args);


#endif