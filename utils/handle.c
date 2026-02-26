#include "handle.h"


static volatile sig_atomic_t RUNNING = 1;
static Socket S = SOCKET_INVALID;
static Set *set = NULL;


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
    printf(" * \x1b[1;32mINFO:\x1b[0m C socket TCP service is running on \x1b[1;36m%s:%d\x1b[0m (Press \x1b[1;33mCTRL+C\x1b[0m to quit)\n", local_ipv4, port);
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
}


void handle_run_server(int port) {
    signal(SIGINT, handle_interrupt);

    os_mkdir("tcp-storage");

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

    set = set_create();
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

    threadpool_destroy(pool, 1);
    putchar('\n');
    set_view(set);
    putchar('\n');
    set_destroy(set);
    socket_destroy();

    return;
    release_server:
        socket_close(S);
        socket_destroy();
}


int handle_handshake(Socket c, _TransportProtocolHeader *meta) {
    if (socket_recv(c, (char *)meta, sizeof(*meta), 0) != sizeof(*meta)) {
        asynclog_warning("receiving file meta header failed.");
        return 1;
    }

    if (strcmp(meta->token, PASSWORD_TOKEN) != 0) {
        asynclog_fatal("invalid token from client.");
        char response[1024];
        char body[] = "<html><body><h1>TCP server is running...</h1></body></html>";
        snprintf(
            response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s", strlen(body), body
        );
        socket_send(c, response, strlen(response), 0);
        socket_shutdown(c);
        return 1;
    }

    // The server is vulnerable to a path traversal dot-dot-slash attack.
    char safe_filename[512];
    snprintf(safe_filename, sizeof(safe_filename), "./tcp-storage/%s", os_basename(meta->filename));

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

    double start_time = os_time();

    if (socket_setopt_timeout(c, 0, 7.0) == SOCKET_INVALID) {
        asynclog_warning("socket sender timeout setting failed.");
        socket_close(c);
        return;
    }

    _TransportProtocolHeader meta;
    if (handle_handshake(c, &meta) == 1) {
        socket_close(c);
        return;
    }

    // The server is vulnerable to a path traversal dot-dot-slash attack.
    char safe_filename[512];
    snprintf(safe_filename, sizeof(safe_filename), "./tcp-storage/%s", os_basename(meta.filename));

    FILE *fp = fopen(safe_filename, "a+b");
    if (!fp) {
        asynclog_warning("file \"%s\" fopen failed.", meta.filename);
        socket_close(c);
        return;
    }

    _ContextMD5 ctx;
    __md5_init__(&ctx);

    unsigned long long filesize = socket_ntohll(meta.filesize);
    unsigned long long offset = os_filesize(safe_filename);

    if (offset > 0 && offset < filesize) {
        FILE *p = fopen(safe_filename, "rb");
        if (p) {
            char temp[BUFFER_CAPACITY];
            unsigned long long read = offset;
            while (read > 0) {
                unsigned long length = fread(temp, 1, (read > sizeof(temp)) ? sizeof(temp) : read, p);
                if (length <= 0) break;
                __md5_update__(&ctx, (unsigned char *)temp, length);
                read = read - length;
            }
            fclose(p);
        }
    }

    int n;
    char buffer[BUFFER_CAPACITY];
    while ((n = socket_recv(c, buffer, sizeof(buffer), 0)) > 0) {
        unsigned long written = fwrite(buffer, 1, n, fp);
        if (written != (unsigned long)n) {
            asynclog_fatal("disk write failed (disk full?) for file \"%s\".", safe_filename);
            break;
        }
        __md5_update__(&ctx, (unsigned char *)buffer, n);
    }
    fflush(fp);

    offset = os_filesize(safe_filename);
    char response[1024];

    if (offset >= filesize) {
        unsigned char digest[16];
        __md5_finalize__(&ctx, digest);
        char hash[33];
        for (unsigned int i = 0; i < 16; ++i) sprintf(hash + (i * 2), "%02x", digest[i]);
        hash[32] = '\0';

        if (strcmp(hash, meta.md5) != 0) {
            remove(safe_filename);
            snprintf(
                response, sizeof(response),
                "[\x1b[1;31mFAILURE\x1b[0m] the MD5 of server and client dismatch, deleting file \"%s\".",
                os_basename(safe_filename)
            );
            asynclog_warning("the MD5 of server and client dismatch, deleting file \"%s\".", os_basename(safe_filename));
            socket_send(c, response, strlen(response), 0);
        } else {
            set_add(set, hash);
            char size[16];
            handle_unit_convert(filesize, size, sizeof(size));
            snprintf(
                response, sizeof(response),
                "[\x1b[1;32mOK\x1b[0m] file \"%s\" has been successfully uploaded:\n"
                " - MD5 hash: %s\n"
                " - File size: %s\n"
                " - Cost time: %.5lf s",
                os_basename(safe_filename), hash, size, os_time() - start_time
            );
            asynclog_info(
                "file \"%s\" has been successfully uploaded:\n"
                " - MD5 hash: %s\n"
                " - File size: %s\n"
                " - Cost time: %.5lf s",
                os_basename(safe_filename), hash, size, os_time() - start_time
            );
            socket_send(c, response, strlen(response), 0);
        }
    } else asynclog_warning("checkpoint progress: %.3lf%%", (double)offset / filesize * 100);

    fclose(fp);
    socket_shutdown(c);
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

    if (socket_setopt_timeout(s, 1, 12.0) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31msocket receiver timeout setting failed.\x1b[0m\n");
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

    char hash[33];
    md5_file(fp, hash);
    fseek(fp, 0, SEEK_SET);

    _TransportProtocolHeader meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.md5, hash, sizeof(meta.md5) - 1);
    strncpy(meta.token, PASSWORD_TOKEN, sizeof(meta.token) - 1);
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

    // int step= (transmission_size / 4096 > 0) ? (transmission_size / 4096) : 1;
    int step = 8192;
    char speed_info[16];
    double start_time = os_time();

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

        double duration = os_time() - start_time;
        double speed = (double)total_sent / (duration + 1e-9);
        char common_speed[16];
        handle_unit_convert((unsigned int)speed, common_speed, sizeof(common_speed));
        snprintf(speed_info, sizeof(speed_info), "%s/s", common_speed);
        handle_progress_bar(total_sent, transmission_size, step, speed_info);
    }

    socket_shutdown(s);

    char response[1024];
    int n = socket_recv(s, response, sizeof(response) - 1, 0);
    if (n <= 0) {
        printf("\x1b[1;37;43m[Warning]\x1b[0m \x1b[1;33mreceiving data failed.\x1b[0m\n");
        fclose(fp);
        goto release_client;
    }
    response[n] = '\0';
    printf("%s\n", response);

    fclose(fp);

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
    *port = 31415;
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