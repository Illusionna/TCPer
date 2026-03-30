#include "parse.h"


void parse_username(char *buffer, char **envs) {
    for (int i = 0; envs[i] != NULL; i++) {
        #if defined(__OS_UNIX__)
            if (strncmp(envs[i], "USER=", 5) == 0) {
                strcpy(buffer, envs[i] + 5);
                return;
            }
        #elif defined(__OS_WINDOWS__)
            if (strncmp(envs[i], "USERNAME=", 9) == 0) {
                strcpy(buffer, envs[i] + 9);
                return;
            }
        #endif
    }
    strcpy(buffer, "UNKNOWN");
}


int parse_port(char *buffer) {
    char *end;
    long port = strtol(buffer, &end, 10);
    if (*end != '\0' || port <= 0 || port > 65535) return 0;
    else return (int)port;
}


int parse_config_file(char *path, char *host_ipv4, int *host_port, char *proxy_ipv4, int *proxy_port) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 1;

    host_ipv4[0] = '\0';
    proxy_ipv4[0] = '\0';
    *host_port = 0;
    *proxy_port = 0;

    char line[64];
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "host ipv4 = %15[0-9.]", host_ipv4) == 1) continue;
        if (sscanf(line, "host port = %d", host_port) == 1) continue;
        if (sscanf(line, "proxy ipv4 = %15[0-9.]", proxy_ipv4) == 1) continue;
        if (sscanf(line, "proxy port = %d", proxy_port) == 1) continue;
    }
    fclose(fp);
    return 0;
}


int parse_config_cmd(char *arg1, char *arg2, char *host_ipv4, int *host_port, char *proxy_ipv4, int *proxy_port) {
    host_ipv4[0] = '\0';
    proxy_ipv4[0] = '\0';
    *host_port = 0;
    *proxy_port = 0;

    int valid_count = 0;

    if (arg1 != NULL) {
        if (sscanf(arg1, "--host=%15[0-9.]:%d", host_ipv4, host_port) == 2) {
            if (socket_valid_ipv4(host_ipv4) && *host_port > 0 && *host_port <= 65535) valid_count++;
            else return 1;
        }
        else if (sscanf(arg1, "--proxy=%15[0-9.]:%d", proxy_ipv4, proxy_port) == 2) {
            if (socket_valid_ipv4(proxy_ipv4) && *proxy_port > 0 && *proxy_port <= 65535) valid_count++;
            else return 1;
        }
        else return 1;
    }

    if (arg2 != NULL) {
        if (sscanf(arg2, "--host=%15[0-9.]:%d", host_ipv4, host_port) == 2) {
            if (socket_valid_ipv4(host_ipv4) && *host_port > 0 && *host_port <= 65535) valid_count++;
            else return 1;
        }
        else if (sscanf(arg2, "--proxy=%15[0-9.]:%d", proxy_ipv4, proxy_port) == 2) {
            if (socket_valid_ipv4(proxy_ipv4) && *proxy_port > 0 && *proxy_port <= 65535) valid_count++;
            else return 1;
        }
        else return 1;
    }

    return valid_count > 0 ? 0 : 1;
}


int parse_config_write(char *path, char *host_ipv4, int host_port, char *proxy_ipv4, int proxy_port) {
    FILE *fp = fopen(path, "w");
    if (!fp) return 1;
    fprintf(fp, "host ipv4 = %s\nhost port = %d\nproxy ipv4 = %s\nproxy port = %d\n", host_ipv4, host_port, proxy_ipv4, proxy_port);
    fclose(fp);
    return 0;
}


void parse_unit_convert(uint64 x, char *buffer, int size) {
    if (x < 1024ULL) snprintf(buffer, size, "%llu B", x);
    else if (x < 1048576ULL) snprintf(buffer, size, "%.3lf KB", x / 1024.0);
    else if (x < 1073741824ULL) snprintf(buffer, size, "%.3lf MB", x / 1024.0 / 1024.0);
    else if (x < 1099511627776ULL) snprintf(buffer, size, "%.3lf GB", x / 1024.0 / 1024.0 / 1024.0);
    else if (x < 1125899906842624ULL) snprintf(buffer, size, "%.3lf TB", x / 1024.0 / 1024.0 / 1024.0 / 1024.0);
    else snprintf(buffer, size, "%.3lf PB", x / 1024.0 / 1024.0 / 1024.0 / 1024.0 / 1024.0);
}