#include "app.h"


static volatile int REMOTE_HOST_PORT = 0;
static volatile sig_atomic_t RUNNING = 1;
static Socket S = SOCKET_INVALID;
static HashMap *dict = NULL;
static Mutex lock;


void app_start_tips(char *ipv4, int port) {
    printf(" * \x1b[1;32mINFO:\x1b[0m started server process ID [\x1b[1;36m%d\x1b[0m]\n", os_getpid());
    printf(" * \x1b[1;32mINFO:\x1b[0m waiting for application startup.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m application startup complete.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m C socket TCP service is running on \x1b[1;36m%s:%d\x1b[0m (press \x1b[1;33mCTRL+C\x1b[0m to quit)\n", ipv4, port);
}


void app_quit_tips() {
    printf(" * \x1b[1;32mINFO:\x1b[0m shutting down.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m waiting for application shutdown.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m application shutdown complete.\n");
    printf(" * \x1b[1;32mINFO:\x1b[0m finished server process ID [\x1b[1;36m%d\x1b[0m]\n", os_getpid());
}


void app_interrupt(int semaphore) {
    (void)semaphore;
    RUNNING = 0;
    if (S != SOCKET_INVALID) socket_close(S);
}


bool app_handshake(Socket c, _TransportProtocolHeader *meta) {
    if (socket_recv(c, (char *)meta, sizeof(*meta), 0) != sizeof(*meta)) {
        asynclog_warning("receiving file meta header failed.");

        char html[1280];
        int html_length = snprintf(
            html, sizeof(html),
            "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\"content=\"width=device-width, initial-scale=1.0\"><title>TCPer</title><style>body{max-width:600px;width:90%%;margin:0 auto;font-family:\"Times New Roman\",Times,serif;margin-top:12vh;color:#1a1a1a;background-color:#fff}h1{font-size:2.2em;font-weight:normal;margin-bottom:0.2em;word-wrap:break-word}h2{font-weight:normal;margin-bottom:0.2em;word-wrap:break-word}p{font-size:1.1em;line-height:1.6}hr{border:0;border-top:1px solid#e5e5e5;margin:30px 0}.footer{font-size:0.9em;color:#777;text-align:center;font-style:italic}@media(max-width:480px){body{margin-top:8vh}h1{font-size:1.8em}p{font-size:1em}}</style></head><body><h1>Welcome to TCPer Service!</h1><p>If you see this page, the TCPer service is successfully running and responding to HTTP request.</p><p>For file transmission, please connect using the TCPer client application.</p><h2>Socket in C = <strong>%s:%d</strong></h2><hr/><div class=\"footer\"><a href=\"https://github.com/Illusionna/TCPer\" target=\"_blank\">https://github.com/Illusionna/TCPer</a> | <a href=\"https://orzzz.net\" target=\"_blank\">Illusionna (Zolio Marling)</a></div></body></html>",
            "0.0.0.0", REMOTE_HOST_PORT
        );

        char response[1536];
        snprintf(
            response, sizeof(response),
            "HTTP/1.1 200 OK\r\n"
            "Server: TCPer/1.0\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s", html_length, html
        );

        socket_send(c, response, strlen(response), 0);
        socket_shutdown(c);
        return False;
    }

    if (strcmp(meta->token, PASSWORD_TOKEN) != 0) {
        asynclog_fatal("refusal to transmit with invalid authentication password token!");
        char status = HANDSHAKE_ERROR_TOKEN;
        socket_send(c, &status, 1, 0);
        socket_shutdown(c);
        return False;
    }

    if (os_traversal(meta->filename)) {
        asynclog_warning("refusal to transmit with unsafe path \"%s\" suspected to traverse boundary!", meta->filename);
        char status = HANDSHAKE_ERROR_PATH;
        socket_send(c, &status, 1, 0);
        socket_shutdown(c);
        return False;
    }

    char status = HANDSHAKE_OK;
    socket_send(c, &status, 1, 0);
    return True;
}


void app_run_service(int port) {
    signal(SIGINT, app_interrupt);

    char workspace[PATH_LENGTH_RESTRICTION];
    if (os_mkdir("tcp-file-storage")) {
        printf("\x1b[1;37;41m[Error]\x1b[0m failed to create the \"tcp-file-storage\" folder in \"%s\"\n", os_getpwd(workspace, sizeof(workspace)));
        return;
    }

    if (socket_init() == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m initialize socket failed.\n");
        return;
    }

    S = socket_create(AF_INET, SOCK_STREAM, 0);
    if (S == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m create socket failed.\n");
        socket_destroy();
        return;
    }

    int recv_buffer_size = BUFFER_CAPACITY;
    if (socket_setopt(S, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, sizeof(recv_buffer_size)) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m socket buffer setting failed.\n");
        goto release_server;
    }

    if (socket_setopt(S, SOL_SOCKET, SO_REUSEADDR, NULL, 0) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m socket port reuse failed.\n");
        goto release_server;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    socket_config(&server, AF_INET, "0.0.0.0", port);
    REMOTE_HOST_PORT = port;

    if (socket_bind(S, &server, sizeof(server)) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m socket bind failed.\n");
        goto release_server;
    }

    if (socket_listen(S, 16) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m socket listen failed.\n");
        goto release_server;
    }

    dict = hashmap_create();
    ThreadPool *pool = threadpool_create(8, 32);
    mutex_create(&lock, 1);

    while (RUNNING) {
        struct sockaddr_in client;
        int client_length = sizeof(client);

        Socket c = socket_accept(S, &client, &client_length);
        if (c == SOCKET_INVALID) {
            if (!RUNNING) break;
            asynclog_warning("socket accept failed.");
            continue;
        }

        asynclog_trace("client from \"%s:%d\"", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        if (threadpool_add(pool, app_thread_task, (void *)uintptr(c), 1, NULL)) {
            asynclog_warning("server overloaded because thread pool adding task failed.");
            const char *response = "HTTP/1.1 503 Service Unavailable\r\nContent-Type: text/plain\r\nContent-Length: 31\r\nConnection: close\r\n\r\nServer is too busy right now...";
            socket_send_nowait(c, (char *)response, strlen(response));
            socket_shutdown(c);
            socket_close(c);
            continue;
        }
    }

    mutex_destroy(&lock);
    threadpool_destroy(pool, 1);
    putchar('\n');
    hashmap_print_view(dict);
    putchar('\n');
    hashmap_destroy(dict);
    socket_destroy();

    return;
    release_server:
        socket_close(S);
        socket_destroy();
}


void app_thread_task(void *args) {
    Socket c = (Socket)uintptr(args);

    double start_time = os_time();

    if (socket_setopt_timeout(c, 0, 7.0) == SOCKET_INVALID) {
        asynclog_warning("socket sender timeout setting failed.");
        socket_close(c);
        return;
    }

    _TransportProtocolHeader meta;
    if (!app_handshake(c, &meta)) {
        socket_close(c);
        return;
    }

    char filepath[PATH_LENGTH_RESTRICTION];
    char temp_file[PATH_LENGTH_RESTRICTION];
    snprintf(filepath, sizeof(filepath), "./tcp-file-storage/%s", os_basename(meta.filename));
    snprintf(temp_file, sizeof(temp_file), "%s_%s.downloading", filepath, meta.md5);

    uint64 total_size = socket_ntohll(meta.filesize);
    int64 finished_size = os_filesize(filepath);
    int64 temp_size = os_filesize(temp_file);

    if (finished_size != -1) {
        if ((uint64)finished_size < total_size) {
            rename(filepath, temp_file);
            temp_size = finished_size;
        }
    }

    uint64 offset = (finished_size != -1 && (uint64)finished_size >= total_size) ? (uint64)finished_size : (temp_size != -1) ? (uint64)temp_size : 0;

    uint64 network_offset = socket_htonll(offset);
    if (socket_send(c, (char *)&network_offset, sizeof(network_offset), 0) != sizeof(network_offset)) {
        asynclog_warning("sending file offset failed.");
        socket_close(c);
        return;
    }

    if (offset >= total_size) {
        asynclog_warning("file \"%s\" already fully received.", meta.filename);
        socket_close(c);
        return;
    }

    char buffer[BUFFER_CAPACITY];
    _ContextMD5 ctx;
    __md5_init__(&ctx);

    if (offset > 0) {
        asynclog_trace("calculating MD5 for existing temporary file via \"os_mmap()\"...");

        MapFile *mf = os_mmap(temp_file, offset); 
        if (mf && mf->data) {
            const uint32 CHUNK_SIZE = 1024 * 1024 * 4;
            uint64 remaining = offset;
            unsigned char *ptr = (unsigned char *)mf->data;

            while (remaining > 0) {
                unsigned int chunk = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
                __md5_update__(&ctx, ptr, chunk);
                ptr = ptr + chunk;
                remaining = remaining - chunk;
            }
            os_munmap(mf);
            asynclog_trace("\"os_munmap()\" MD5 calculation finished.");
        } else {
            asynclog_warning("\"os_mmap()\" failed for \"%s\", falling back to \"fread()\".", temp_file);
            FILE *temp_fp = fopen(temp_file, "rb");
            if (!temp_fp) {
                asynclog_warning("temporary file \"%s\" open for reading failed.", temp_file);
                socket_close(c);
                return;
            }
            char temp_buffer[BUFFER_CAPACITY];
            for (uint64 read = offset; read > 0; ) {
                usize length = fread(temp_buffer, 1, read > sizeof(temp_buffer) ? sizeof(temp_buffer) : read, temp_fp);
                if (length <= 0) break;
                __md5_update__(&ctx, (unsigned char *)temp_buffer, (unsigned int)length);
                read = read - length;
            }
            fclose(temp_fp);
        }
    }

    FILE *fp = fopen(temp_file, "a+b");
    if (!fp) {
        asynclog_warning("temp file \"%s\" fopen failed.", temp_file);
        socket_close(c);
        return;
    }
    os_fseek(fp, 0, SEEK_END);

    bool ok = True;
    uint64 received_bytes = 0;
    uint64 expected_size = total_size - offset;

    while (received_bytes < expected_size) {
        long n = socket_recv(
            c,
            buffer,
            expected_size - received_bytes > sizeof(buffer) ? sizeof(buffer) : expected_size - received_bytes,
            0
        );
        if (n <= 0) {
            asynclog_warning("receiving data failed.");
            ok = False;
            break;
        }
        if (fwrite(buffer, 1, n, fp) != (usize)n) {
            asynclog_fatal("file \"%s\" disk write failed.", temp_file);
            ok = False;
            break;
        }
        __md5_update__(&ctx, (unsigned char *)buffer, (unsigned int)n);
        received_bytes = received_bytes + n;
    }

    fflush(fp);
    fclose(fp);

    if (!ok) {
        asynclog_warning("incomplete transmission to keep temporary file for resume.");
        socket_close(c);
        return;
    }

    int64 actual_size = os_filesize(temp_file);
    if (actual_size == -1) {
        asynclog_warning("get actual size of temp file failed.");
        socket_close(c);
        return;
    }

    if ((uint64)actual_size >= total_size) {
        char hash[33];
        unsigned char digest[16];
        __md5_finalize__(&ctx, digest);
        for (unsigned int i = 0; i < 16; ++i) snprintf(hash + (i * 2), 3, "%02x", digest[i]);
        hash[32] = '\0';

        char response[1024];
        if (strcmp(hash, meta.md5) != 0) {
            remove(temp_file);
            snprintf(
                response, sizeof(response),
                "[\x1b[1;31mFAILURE\x1b[0m] delete the \"%s\" file that does dismatch the MD5 checksum.",
                os_basename(filepath)
            );
            asynclog_warning("delete the \"%s\" file that does dismatch the MD5 checksum.", os_basename(filepath));
        } else if (rename(temp_file, filepath) == 0) {
            mutex_lock(&lock);
            hashmap_add(dict, hash, filepath);
            mutex_unlock(&lock);
            char temp[16];
            parse_unit_convert(total_size, temp, sizeof(temp));
            snprintf(
                response, sizeof(response),
                "[\x1b[1;32mOK\x1b[0m] file \"%s\" has been successfully uploaded:\n"
                " - MD5 hash: %s\n"
                " - File size: %s\n"
                " - Cost time: %.5lf s",
                os_basename(filepath), hash, temp, os_time() - start_time
            );
            asynclog_info(
                "file \"%s\" has been successfully uploaded:\n"
                " - MD5 hash: %s\n"
                " - File size: %s\n"
                " - Cost time: %.5lf s",
                os_basename(filepath), hash, temp, os_time() - start_time
            );
        } else {
            snprintf(response, sizeof(response), "[\x1b[1;31mFAILURE\x1b[0m] rename failed for \"%s\".", os_basename(filepath));
            asynclog_warning("failed to rename temp file to \"%s\".", filepath);
        }
        // Send the '\0' with `strlen(response) + 1` at the end to inform the client to stop the transmission.
        socket_send(c, response, strlen(response) + 1, 0);
        socket_shutdown(c);
    } else {
        asynclog_warning("checkpoint progress: %.3lf%%", (double)actual_size / total_size * 100);
    }

    socket_close(c);
}


void app_client_send(char *path, char *host_ipv4, int host_port, char *proxy_ipv4, int proxy_port) {
    if (socket_init() == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m initialize socket failed.\n");
        return;
    }

    Socket s = socket_create(AF_INET, SOCK_STREAM, 0);
    if (s == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m create socket failed.\n");
        socket_destroy();
        return;
    }

    FILE *fp = NULL;

    int send_buffer_size = BUFFER_CAPACITY;
    if (socket_setopt(s, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size)) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m socket buffer setting failed.\n");
        goto release_client;
    }

    if (socket_setopt_timeout(s, 1, 7.0) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m socket receiver timeout setting failed.\n");
        goto release_client;
    }

    if (socket_setopt(s, IPPROTO_TCP, TCP_NODELAY, NULL, 0) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m disable Nagle algorithm failed.\n");
        goto release_client;
    }

    if (!proxy_ipv4 && proxy_port == 0) {
        struct sockaddr_in direct_server;
        memset(&direct_server, 0, sizeof(direct_server));
        socket_config(&direct_server, AF_INET, host_ipv4, host_port);

        if (socket_connect_timeout(s, &direct_server, sizeof(direct_server), 3.7) == SOCKET_INVALID) { 
            printf("\x1b[1;37;41m[Error]\x1b[0m is the server \"%s:%d\" running?\n", host_ipv4, host_port);
            goto release_client;
        }
    } else {
        struct sockaddr_in proxy_server;
        memset(&proxy_server, 0, sizeof(proxy_server));
        socket_config(&proxy_server, AF_INET, proxy_ipv4, proxy_port);

        if (socket_connect_timeout(s, &proxy_server, sizeof(proxy_server), 3.7) == SOCKET_INVALID) {
            printf("\x1b[1;37;41m[Error]\x1b[0m is the proxy \"%s:%d\" running?\n", proxy_ipv4, proxy_port);
            goto release_client;
        }

        char authentication_request[] = {0x05, 0x01, 0x00};
        if (socket_send(s, authentication_request, 3, 0) != 3) {
            printf("\x1b[1;37;41m[Error]\x1b[0m proxy authentication request failed.\n");
            goto release_client;
        }

        char authentication_response[2];
        if (
            socket_recv(s, authentication_response, 2, 0) != 2
            ||
            authentication_response[0] != 0x05
            ||
            authentication_response[1] != 0x00
        ) {
            printf("\x1b[1;37;41m[Error]\x1b[0m proxy authentication rejected.\n");
            goto release_client;
        }

        char vpn_request[10];
        vpn_request[0] = 0x05;
        vpn_request[1] = 0x01;
        vpn_request[2] = 0x00;
        vpn_request[3] = 0x01;

        struct in_addr remote_ipv4;
        inet_pton(AF_INET, host_ipv4, &remote_ipv4);
        memcpy(&vpn_request[4], &remote_ipv4.s_addr, 4);
        unsigned short remote_port = socket_htons(host_port);
        memcpy(&vpn_request[8], &remote_port, 2);

        if (socket_send(s, vpn_request, 10, 0) != 10) {
            printf("\x1b[1;37;41m[Error]\x1b[0m proxy connect request failed.\n");
            goto release_client;
        }

        char vpn_response[260];

        // Read the first 4 bytes (`VER`, `REP`, `RSV`, `ATYP`) and check the status code `REP`.
        if (socket_recv(s, vpn_response, 4, 0) != 4 || vpn_response[1] != 0x00) {
            printf("\x1b[1;37;41m[Error]\x1b[0m proxy \"%s:%d\" route to remote server \"%s:%d\" failed.\n", proxy_ipv4, proxy_port, host_ipv4, host_port);
            goto release_client;
        }

        // `0x01` for IPv4 has 6 remaining bytes; `0x04` for IPv6 has 18 remaining bytes.
        int remain = (vpn_response[3] == 0x01) ? 6 : (vpn_response[3] == 0x04) ? 18 : 0;

        // `0x03` for domain name requires the length of 1 byte, followed by 2 bytes for the port.
        if (vpn_response[3] == 0x03) {
            if (socket_recv(s, vpn_response, 1, 0) != 1) goto release_client;
            // prevent the length of the long domain name from becoming negative.
            remain = (unsigned char)vpn_response[0] + 2;
        }

        // Discard all the remaining garbage data at once to ensure that TCP buffer is clean.
        if (remain > 0 && socket_recv(s, vpn_response, remain, 0) != remain) {
            printf("\x1b[1;37;41m[Error]\x1b[0m proxy remaining response read failed.\n");
            goto release_client;
        }

        printf("\x1b[1;37;42m[INFO]\x1b[0m proxy Socks5 VPN Tunnel \"%s:%d\" to remote server \"%s:%d\" established.\n", proxy_ipv4, proxy_port, host_ipv4, host_port);
    }

    fp = fopen(path, "rb");
    if (!fp) {
        printf("\x1b[1;37;41m[Error]\x1b[0m file \"%s\" open failed\n", path);
        goto release_client;
    }

    char hash[33];
    md5_file(fp, hash);
    os_fseek(fp, 0, SEEK_SET);

    _TransportProtocolHeader meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.md5, hash, sizeof(meta.md5) - 1);
    strncpy(meta.token, PASSWORD_TOKEN, sizeof(meta.token) - 1);
    meta.token[sizeof(meta.token) - 1] = '\0';
    strncpy(meta.filename, os_basename(path), sizeof(meta.filename) - 1);
    meta.filename[sizeof(meta.filename) - 1] = '\0';

    int64 temp_size = os_filesize(path);
    if (temp_size == -1) {
        printf("get the size of \"%s\" failed.\n", path);
        goto release_client;
    }
    uint64 total_size = (uint64)temp_size;
    meta.filesize = socket_htonll(total_size);

    if (socket_send(s, (char *)&meta, sizeof(meta), 0) != sizeof(meta)) {
        printf("\x1b[1;37;41m[Error]\x1b[0m sending file meta header failed.\n");
        goto release_client;
    }

    char status;
    if (socket_recv(s, &status, 1, 0) != 1) {
        printf("\x1b[1;37;41m[Error]\x1b[0m receiving handshake status failed.\n");
        goto release_client;
    }

    if (status != HANDSHAKE_OK) {
        if (status == HANDSHAKE_ERROR_TOKEN) printf("\x1b[1;37;41m[Error]\x1b[0m invalid authentication password token!\n");
        else if (status == HANDSHAKE_ERROR_PATH) printf("\x1b[1;37;41m[Error]\x1b[0m unsafe path suspected to traverse boundary.\n");
        else printf("\x1b[1;37;41m[Error]\x1b[0m unknown handshake error (0x%02x).\n", status);
        goto release_client;
    }

    uint64 network_offset;
    if (socket_recv(s, (char *)&network_offset, sizeof(network_offset), 0) != sizeof(network_offset)) {
        printf("\x1b[1;37;41m[Error]\x1b[0m receiving file meta header offset failed.\n");
        goto release_client;
    }

    uint64 offset = socket_ntohll(network_offset);
    uint64 transmission_size = total_size - offset;

    if (offset >= total_size) {
        printf("\x1b[1;37;42m[Success]\x1b[0m file \"%s\" already exists.\n", os_basename(path));
        goto release_client;
    }
    if (offset > 0) {
        printf("\x1b[1;37;42m[INFO]\x1b[0m resumable transmission from \x1b[1;32m[%llu]\x1b[0m to \x1b[1;32m[%llu]\x1b[0m bytes.\n", offset, total_size);
        os_fseek(fp, (int64)offset, SEEK_SET);
    }

    printf("\x1b[1;37;42m[INFO]\x1b[0m awaiting...\n");

    #if defined(__linux__)
        long long sent_bytes = socket_sendfile(s, fp, offset, transmission_size);
        if (sent_bytes < 0) {
            printf("\x1b[1;37;41m[Error]\x1b[0m remaining data sending failed.\n");
            goto release_client;
        }
    #else
        int step = BUFFER_CAPACITY;
        char description[32] = "1.000 GB/s";
        char speed_buffer[16];
        char buffer[BUFFER_CAPACITY];
        uint64 sent_bytes = 0;
        usize length;
        uint64 last_bytes = 0;
        double last_time = os_time();

        while ((length = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
            char *p = buffer;
            while (length > 0) {
                long n = socket_send(s, p, length, 0);
                if (n <= 0) {
                    printf("\x1b[1;37;41m[Error]\x1b[0m remaining data sending failed.\n");
                    goto release_client;
                }
                p = p + n;
                length = length - n;
                sent_bytes = sent_bytes + n;
            }

            double current_time = os_time();
            double interval = current_time - last_time;

            if (interval >= 0.2) {
                double speed = (double)(sent_bytes - last_bytes) / interval;
                parse_unit_convert((uint64)speed, speed_buffer, sizeof(speed_buffer));
                snprintf(description, sizeof(description), "%s/s", speed_buffer);
                last_time = current_time;
                last_bytes = sent_bytes;
            }
            usage_progress_bar(sent_bytes, transmission_size, step, description);
        }
    #endif

    char response[1024];
    usize accumulation = 0;
    while (accumulation < sizeof(response) - 1) {
        long n = socket_recv(s, response + accumulation, sizeof(response) - accumulation - 1, 0);
        if (n <= 0) break;
        accumulation = accumulation + n;
        // Once client detects '\0' received, it immediately exits on its own initiative.
        if (response[accumulation - 1] == '\0') {
            accumulation = accumulation - 1;
            break;
        }
    }
    response[accumulation] = '\0';

    if (accumulation == 0) printf("\x1b[1;37;43m[Warning]\x1b[0m receiving socket data failed.\n");
    else printf("%s\n", response);

    release_client:
        if (fp) fclose(fp);
        if (s != SOCKET_INVALID) {
            socket_close(s);
            socket_destroy();
        }
}