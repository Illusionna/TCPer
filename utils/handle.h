#ifndef _HANDLER_H_
#define _HANDLER_H_


#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>


#include "socket.h"
#include "threadpool.h"
#include "async_log.h"
#include "set.h"
#include "md5.h"
#include "os.h"


#define DEFAULT_PORT 31415
#define BUFFER_CAPACITY 65536
#define PASSWORD_TOKEN "THIS_IS_A_TCP_TOKEN_GENERATED_BY_ILLUSIONNA"


typedef struct {
    char md5[33];
    char token[64];
    char filename[256];
    unsigned long long filesize;
} _TransportProtocolHeader;


void handle_usage_logo();


void handle_usage_help(char *app_name, int port);


void handle_usage_start(int port, char *local_ipv4);


void handle_usage_quit();


void handle_unit_convert(unsigned int value, char *buffer, int size);


void handle_progress_bar(int current, int epoch, int step, char *description);


void handle_interrupt(int semaphore);


void handle_run_server(int port);


void handle_run_client(char *filepath, char *ipv4, int port);


void handle_thread_task(void *args);


int handle_read_config(char *ip, int *port);


int handle_parse_config(char *address, char *ipv4, int *port);


void handle_write_config(char *ip, int port);


int handle_handshake(Socket c, _TransportProtocolHeader *meta);


#endif