#include "handle.h"


static volatile sig_atomic_t RUNNING = 1;
static Socket S = SOCKET_INVALID;


void handle_usage_logo() {
    printf(
        "\x1b[1;37m   _\x1b[0m\n"
        "\x1b[1;31m  ( \\                ..-----..__\x1b[0m\n"
        "\x1b[1;31m   \\.'.        _.--'`  [   '  ' ```'-._\x1b[0m\n"
        "\x1b[1;32m    `. `'-..-'' `    '  ' '   .  ;   ; `-'''-.,__/|/_\x1b[0m\n"
        "\x1b[1;32m      `'-.;..-''`|'  `.  '.    ;     '  `    '   `'  `,\x1b[0m\n"
        "\x1b[1;33m                 \\ '   .    ' .     '   ;   .`   . ' 7 \\\x1b[0m\n"
        "\x1b[1;33m                  '.' . '- . \\    .`   .`  .   .\\     `Y\x1b[0m\n"
        "\x1b[1;34m                    '-.' .   ].  '   ,    '    /'`\"\"';:'\x1b[0m\n"
        "\x1b[1;34m                      /Y   '.] '-._ /    ' _.-'\x1b[0m\n"
        "\x1b[1;35m                      \\'\\_   ; (`'.'.'  .\"/\"\x1b[0m\n"
        "\x1b[1;35m                       ' )` /  `.'   .-'.'\x1b[0m\n"
        "\x1b[1;36m                        '\\  \\).'  .-'--\"\x1b[0m\n"
        "\x1b[1;36m                          `. `,_'`\x1b[0m\n"
        "\x1b[1;37m                            `.__)\x1b[0m\n"
        " _________    ______  _______   \n"
        "|  _   _  | .' ___  ||_   __ \\     https://github.com/Illusionna/TCPer\n"
        "|_/ | | \\_|/ .'   \\_|  | |__) |    can transmit quickly to upload your file.\n"
        "    | |    | |         |  ___/     available version 1.0\n"
        "   _| |_   \\ `.___.'\\ _| |_        Illusionna (Zolio Marling) www@orzzz.net\n"
        "  |_____|   `.____ .'|_____|   -------------------------------------------------\n"
    );
}


void handle_usage_help(char *app_name, int port) {
    printf(" * (server) >>> \x1b[1;32m%s run [%d]\x1b[0m\n", app_name, port);
    printf(" * (client) >>> \x1b[1;32m%s config [IPv4:%d]\x1b[0m\n", app_name, port);
    printf(" * (client) >>> \x1b[1;32m%s send [filepath]\x1b[0m\n", app_name);
}


void handle_usage_start(int port, char *local_ipv4) {
    printf(" * \x1b[1;32mINFO:\x1b[0m Started server process ID [\x1b[1;36m%d\x1b[0m]\n", os_getpid());
    printf(" * \x1b[1;32mINFO:\x1b[0m Waiting for application startup.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m Application startup complete.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m C socket TCP service running on \x1b[1;36mhttp://%s:%d\x1b[0m (Press \x1b[1;33mCTRL+C\x1b[0m to quit)\n", local_ipv4, port);
}


void handle_usage_quit() {
    printf(" * \x1b[1;32mINFO:\x1b[0m Shutting down.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m Waiting for application shutdown.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m Application shutdown complete.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m Finished server process ID [\x1b[1;36m%d\x1b[0m]\n", os_getpid());
}


void handle_unit_convert(unsigned int value, char *buffer, int size) {
    if (value < 1024) snprintf(buffer, size, "%u B", value);
    else if (value < 1048576) snprintf(buffer, size, "%.2f KB", value / 1024.0);
    else if (value < 1073741824) snprintf(buffer, size, "%.2f MB", value / 1024.0 / 1024.0);
    else snprintf(buffer, size, "%.2f GB", value / 1024.0 / 1024.0 / 1024.0);
}


void handle_progress_bar(int current, int epoch, int step, char *description) {
    if (current != 0 && current % step != 0 && current != epoch) return;
    int width = 50;
    // printf("\x1b[?25l");
    float percentage = (epoch > 0) ? (float)current / epoch : 0.0F;
    int filled = percentage * width;
    printf("\r[");
    for (int j = 0; j < width; j++) {
        if (j < filled) printf("\x1b[32m=\x1b[0m");
        else if (j == filled) printf(">");
        else printf(" ");
    }
    printf("] %7.2f%% (%d/%d)", percentage * 100, current, epoch);
    if (description != NULL && description[0] != '\0') printf(" \x1b[33m%s\x1b[0m\x1b[K", description);
    else printf("\x1b[K");
    fflush(stdout);
    if (current >= epoch) printf("\n");
    // if (current >= epoch) printf("\x1b[?25h\n");
}


void handle_interrupt(int semaphore) {
    RUNNING = 0;
    if (S != SOCKET_INVALID) socket_close(S);
    socket_destroy();
}


void handle_run_server(int port) {
    signal(SIGINT, handle_interrupt);

    if (socket_init()) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31minitialize socket failed.\x1b[0m\n");
        return;
    }

    S = socket_create(AF_INET, SOCK_STREAM, 0);
    if (S == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31mcreate socket failed.\x1b[0m\n");
        socket_destroy();
        return;
    }

    int recv_buffer_size = BUFFER_CAPACITY;
    if (socket_setopt(S, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, sizeof(recv_buffer_size)) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31msocket buffer setting failed.\x1b[0m\n");
        goto release_server;
    }

    if (socket_setopt(S, SOL_SOCKET, SO_REUSEADDR, NULL, 0) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31msocket port reuse failed.\x1b[0m\n");
        goto release_server;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    socket_config(&server, AF_INET, "0.0.0.0", port);

    if (socket_bind(S, &server, sizeof(server)) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31msocket bind failed.\x1b[0m\n");
        goto release_server;
    }

    if (socket_listen(S, 16) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31msocket listen failed.\x1b[0m\n");
        goto release_server;
    }

    ThreadPool *pool = threadpool_create(8, 32);

    while (RUNNING) {
        struct sockaddr_in client;
        int client_length = sizeof(client);

        Socket c = socket_accept(S, &client, &client_length);
        if (c == SOCKET_INVALID) {
            if (RUNNING) asynclog_warning("socket accept failed.");
            continue;
        }

        asynclog_trace("client from %s:%d", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        if (threadpool_add(pool, handle_thread_task, (void *)(intptr_t)c, 1, NULL)) {
            asynclog_warning("thread pool adding task failed.");
            socket_close(c);
            continue;
        }
    }

    putchar('\n');
    threadpool_destroy(pool, 1);

    release_server:
        socket_close(S);
        socket_destroy();
}


int handle_handshake(Socket c, _TransportProtocolHeader *meta) {
    if (socket_recv(c, (char *)meta, sizeof(*meta), 0) != sizeof(*meta)) {
        asynclog_warning("receiving file meta header failed.");
        return 1;
    }

    if (strcmp(meta->token, "THIS_IS_A_TCP_TOKEN") != 0) {
        asynclog_fatal("invalid token from client.");
        return 1;
    }

    // The server is vulnerable to a path traversal dot-dot-slash attack.
    char *safe_filename = os_basename(meta->filename);

    unsigned long long filesize = socket_ntohll(meta->filesize);
    unsigned long long offset = os_filesize(safe_filename);
    unsigned long long network_offset = socket_htonll(offset);

    if (socket_send(c, (char *)&network_offset, sizeof(network_offset), 0) != sizeof(network_offset)) {
        asynclog_warning("sending offset failed.");
        return 1;
    }

    if (offset >= filesize) {
        asynclog_warning("file \"%s\" already fully received.", meta->filename);
        return 1;
    }

    return 0;
}


void handle_thread_task(void *args) {
    Socket c = (Socket)(intptr_t)args;

    _TransportProtocolHeader meta;
    if (handle_handshake(c, &meta) == 1) {
        socket_close(c);
        return;
    }

    // The server is vulnerable to a path traversal dot-dot-slash attack.
    char *safe_filename = os_basename(meta.filename);

    // char final_path[512];
    // snprintf(final_path, sizeof(final_path), "./uploads/%s", safe_filename);

    FILE *fp = fopen(safe_filename, "ab");
    if (!fp) {
        asynclog_warning("file \"%s\" fopen failed.", meta.filename);
        socket_close(c);
        return;
    }

    int n;
    char buffer[BUFFER_CAPACITY];
    while ((n = socket_recv(c, buffer, sizeof(buffer), 0)) > 0) {
        unsigned long written = fwrite(buffer, 1, n, fp);
        if (written != (unsigned long)n) {
            asynclog_fatal("disk write failed (disk full?) for file \"%s\".", safe_filename);
            break;
        }
    }

    char hash[33];
    md5_file(fp, hash);
    asynclog_info("server hash = %s\n", hash);

    fclose(fp);
    socket_close(c);
}


void handle_run_client(char *filepath, char *ipv4, int port) {
    if (socket_init()) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31minitialize socket failed.\x1b[0m\n");
        return;
    }

    Socket s = socket_create(AF_INET, SOCK_STREAM, 0);
    if (s == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31mcreate socket failed.\x1b[0m\n");
        socket_destroy();
        return;
    }

    int send_buffer_size = BUFFER_CAPACITY;
    if (socket_setopt(s, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size)) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31msocket buffer setting failed.\x1b[0m\n");
        goto release_client;
    }

    struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};
    if (socket_setopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31msocket timeout setting failed.\x1b[0m\n");
        goto release_client;
    }
    if (socket_setopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31msocket timeout setting failed.\x1b[0m\n");
        goto release_client;
    }

    if (socket_setopt(s, IPPROTO_TCP, TCP_NODELAY, NULL, 0) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31mdisable Nagle algorithm failed.\x1b[0m\n");
        goto release_client;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    socket_config(&server, AF_INET, ipv4, port);

    if (socket_connect(s, &server, sizeof(server)) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31mis the server running?\x1b[0m\n");
        goto release_client;
    }

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        printf("\x1b[1;37;41m[Error]\x1b[0m file open failed\n");
        goto release_client;
    }

    _TransportProtocolHeader meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.token, "THIS_IS_A_TCP_TOKEN", sizeof(meta.token) - 1);
    strncpy(meta.filename, os_basename(filepath), sizeof(meta.filename) - 1);
    unsigned long long filesize = os_filesize(filepath);
    meta.filesize = socket_htonll(filesize);

    if (socket_send(s, (char *)&meta, sizeof(meta), 0) != sizeof(meta)) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31msending file meta header failed.\x1b[0m\n");
        fclose(fp);
        goto release_client;
    }

    unsigned long long network_offset = 0;
    if (socket_recv(s, (char *)&network_offset, sizeof(network_offset), 0) != sizeof(network_offset)) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31mreceiving file meta header offset failed.\x1b[0m\n");
        fclose(fp);
        goto release_client;
    }
    unsigned long long offset = socket_ntohll(network_offset);
    unsigned long long transmission_size = filesize - offset;

    if (offset >= filesize) {
        printf("\x1b[1;37;42m[Success]\x1b[0m \x1b[1;32mfile \"%s\" already exists.\x1b[0m\n", os_basename(filepath));
        fclose(fp);
        goto release_client;
    }
    if (offset > 0) {
        printf("\x1b[1;37;42m[INFO]\x1b[0m \x1b[1;32mresumable transmission from [%llu] to [%llu] bytes.\x1b[0m\n", offset, filesize);
        os_fseek(fp, offset, SEEK_SET);
    }

    char buffer[BUFFER_CAPACITY];
    unsigned long read_length;
    unsigned long total_sent = 0;

    int step = (meta.filesize / 32 > 0) ? (meta.filesize / 32) : 1;

    while ((read_length = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        char *p = buffer;
        unsigned long remaining = read_length;
        while (remaining > 0) {
            long send_length = socket_send(s, p, remaining, 0);
            if (send_length <= 0) {
                printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31mremaining data sending failed.\x1b[0m\n");
                fclose(fp);
                goto release_client;
            }
            p = p + send_length;
            remaining = remaining - send_length;
            total_sent = total_sent + send_length;
        }
        handle_progress_bar(total_sent, transmission_size, step, NULL);
    }

    char hash[33];
    md5_file(fp, hash);
    printf("client hash = %s\n", hash);

    fclose(fp);
    goto release_client;

    release_client:
        socket_close(s);
        socket_destroy();
}


int handle_read_config(char *ip, int *port) {
    char exe_dir[256];
    char exe_path[256 + 128];
    os_getexec(exe_dir, sizeof(exe_dir));
    snprintf(exe_path, sizeof(exe_path), "%s/%s", exe_dir, ".tcp-config");
    FILE *fp = fopen(exe_path, "r");
    if (fp == NULL) return 1;
    char ipv4[32];
    int number;
    if (fscanf(fp, "%15[0-9.] %d", ipv4, &number) != 2) {
        fclose(fp);
        return 1;
    }
    strcpy(ip, ipv4);
    *port = number;
    fclose(fp);
    return 0;
}


int handle_parse_config(char *address, char *ipv4, int *port) {
    strcpy(ipv4, "127.0.0.1");
    *port = 9090;
    if (!address) return 1;
    char ip[32];
    int number;
    if (sscanf(address, "%15[0-9.]:%d", ip, &number) == 2) {
        strcpy(ipv4, ip);
        *port = number;
        return 0;
    }
    return 1;
}


void handle_write_config(char *ip, int port) {
    char exe_dir[256];
    char exe_path[256 + 128];
    os_getexec(exe_dir, sizeof(exe_dir));
    snprintf(exe_path, sizeof(exe_path), "%s/%s", exe_dir, ".tcp-config");
    FILE *fp = fopen(exe_path, "w");
    if (fp == NULL) {
        perror("\x1b[1;37;41m[Error]\x1b[0m");
        return;
    }
    fprintf(fp, "%s\n%d", ip, port);
    fclose(fp);
}