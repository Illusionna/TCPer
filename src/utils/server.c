#include "app.h"


static volatile int REMOTE_HOST_PORT = 0;
static volatile sig_atomic_t RUNNING = 1;
static Socket S = SOCKET_INVALID;
static HashMap *dict = NULL;
static Mutex lock;
static char REMOTE_HOST_IPV4[32] = "0.0.0.0";


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
            REMOTE_HOST_IPV4, REMOTE_HOST_PORT
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

    // `rand()` and `srand()` are not thread-safe. Just for demonstration.
    os_srand();
    byte nonce[16];
    for (int i = 0; i < 16; i++) nonce[i] = rand() % 256;

    if (socket_send(c, (char *)nonce, 16, 0) != 16) {
        asynclog_warning("sending challenge nonce failed.");
        socket_shutdown(c);
        return False;
    }

    byte response[16];
    if (socket_recv(c, (char *)response, 16, 0) != 16) {
        asynclog_warning("receiving challenge response failed.");
        socket_shutdown(c);
        return False;
    }

    parse_rc4((byte *)PASSWORD_TOKEN, strlen(PASSWORD_TOKEN), nonce, 16);

    if (memcmp(response, nonce, 16) != 0) {
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
    socket_ipv4(REMOTE_HOST_IPV4, sizeof(REMOTE_HOST_IPV4));

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

    switch (meta.command) {
        case APP_COMMAND_SEND:
            asynclog_info("execute APP_COMMAND_SEND for file: \"%s\".", meta.filename);
            app_server_send(c, &meta);
            break;
        case APP_COMMAND_LIST:
            asynclog_info("execute APP_COMMAND_LIST.");
            app_server_list(c, &meta);
            break;
        case APP_COMMAND_DOWNLOAD:
            asynclog_info("execute APP_COMMAND_DOWNLOAD for file: \"%s\".", meta.filename);
            app_server_download(c, &meta);
            break;
        case APP_COMMAND_REMOVE:
            asynclog_info("execute APP_COMMAND_REMOVE for file: \"%s\".", meta.filename);
            app_server_remove(c, &meta);
            break;
        default:
            asynclog_warning("Unknown command received: 0x%02x.", meta.command);
            app_server_default(c);
            break;
    }

    socket_close(c);
}


static bool __recv_full__(Socket c, char *buffer, uint64 want) {
    uint64 got = 0;
    while (got < want) {
        long n = socket_recv(c, buffer + got, (int)(want - got), 0);
        if (n <= 0) return False;
        got = got + (uint64)n;
    }
    return True;
}


void app_server_send(Socket c, _TransportProtocolHeader *meta) {
    double start_time = os_time();

    char filepath[PATH_LENGTH_RESTRICTION];
    char temp_file[PATH_LENGTH_RESTRICTION];
    snprintf(filepath, sizeof(filepath), "./tcp-file-storage/%s", os_basename(meta->filename));
    snprintf(temp_file, sizeof(temp_file), "%s_%s.downloading", filepath, meta->md5);

    uint64 total_size = socket_ntohll(meta->filesize);
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
        return;
    }

    if (offset >= total_size) {
        asynclog_warning("file \"%s\" already fully received.", meta->filename);
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
                return;
            }
            char temp_buffer[BUFFER_CAPACITY];
            for (uint64 read = offset; read > 0; ) {
                usize length = fread(temp_buffer, 1, read > sizeof(temp_buffer) ? sizeof(temp_buffer) : read, temp_fp);
                if (length == 0) break;
                __md5_update__(&ctx, (unsigned char *)temp_buffer, (unsigned int)length);
                read = read - length;
            }
            fclose(temp_fp);
        }
    }

    FILE *fp = fopen(temp_file, "a+b");
    if (!fp) {
        asynclog_warning("temp file \"%s\" fopen failed.", temp_file);
        return;
    }
    os_fseek(fp, 0, SEEK_END);

    bool ok = True;
    uint64 received_bytes = 0;
    uint64 expected_size = total_size - offset;

    while (received_bytes < expected_size) {
        uint64 want = (expected_size - received_bytes) > sizeof(buffer) ? sizeof(buffer) : expected_size - received_bytes;
        if (!__recv_full__(c, buffer, want)) {
            ok = False;
            break;
        }

        parse_rc4((byte *)PASSWORD_TOKEN, strlen(PASSWORD_TOKEN), (byte *)buffer, (int)want);

        if (fwrite(buffer, 1, want, fp) != (usize)want) {
            asynclog_fatal("file \"%s\" disk write failed.", temp_file);
            ok = False;
            break;
        }
        __md5_update__(&ctx, (unsigned char *)buffer, (unsigned int)want);
        received_bytes = received_bytes + want;
    }

    fflush(fp);
    fclose(fp);

    if (!ok) {
        asynclog_warning("incomplete transmission to keep temporary file for resume.");
        return;
    }

    int64 actual_size = os_filesize(temp_file);
    if (actual_size == -1) {
        asynclog_warning("get actual size of temp file failed.");
        return;
    }

    if ((uint64)actual_size >= total_size) {
        char hash[33];
        unsigned char digest[16];
        __md5_finalize__(&ctx, digest);
        for (unsigned int i = 0; i < 16; ++i) snprintf(hash + (i * 2), 3, "%02x", digest[i]);
        hash[32] = '\0';

        char response[1024];
        if (strcmp(hash, meta->md5) != 0) {
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
}


void app_server_list(Socket c, _TransportProtocolHeader *meta) {
    if (meta->filename[0] == '/' || meta->filename[0] == '\\') {
        asynclog_warning("refusal to process absolute path request: \"%s\".", meta->filename);
        socket_send(c, "\x1b[1;37;41m[Error]\x1b[0m unsafe path suspected to traverse boundary.", 65, 0);
        socket_shutdown(c);
        return;
    }

    char dir[PATH_LENGTH_RESTRICTION];
    if (strlen(meta->filename) == 0 || strcmp(meta->filename, ".") == 0) snprintf(dir, sizeof(dir), "tcp-file-storage");
    else snprintf(dir, sizeof(dir), "./tcp-file-storage/%s", meta->filename);

    if (os_access(dir) == 1) {
        os_listdir(dir, __app_server_list__, (void *)uintptr(c));
        socket_send(c, "", 1, 0);
    } else socket_send(c, "No such file or directory.", 27, 0);
    socket_shutdown(c);
}


void app_server_download(Socket c, _TransportProtocolHeader *meta) {
    char filepath[PATH_LENGTH_RESTRICTION];

    snprintf(filepath, sizeof(filepath), "./tcp-file-storage/%s", os_basename(meta->filename));

    int64 size = os_filesize(filepath);
    if (size == -1) {
        asynclog_warning("Download failed: file \"%s\" not found.", meta->filename);
        uint64 error_size = 0;
        uint64 network_error_size = socket_htonll(error_size);
        socket_send(c, (char *)&network_error_size, sizeof(network_error_size), 0);
        return;
    }

    uint64 total_size = (uint64)size;
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        asynclog_fatal("Failed to open file \"%s\" for reading.", filepath);
        socket_shutdown(c);
        return;
    }

    char hash[33];
    md5_file(fp, hash);

    uint64 network_total_size = socket_htonll(total_size);
    if (socket_send(c, (char *)&network_total_size, sizeof(network_total_size), 0) != sizeof(network_total_size)) {
        fclose(fp);
        return;
    }
    socket_send(c, hash, 32, 0);

    uint64 network_offset;
    if (socket_recv(c, (char *)&network_offset, sizeof(network_offset), 0) != sizeof(network_offset)) {
        asynclog_warning("Failed to receive offset from client.");
        fclose(fp);
        return;
    }
    uint64 offset = socket_ntohll(network_offset);

    if (offset >= total_size) {
        asynclog_info("Client already has the full file.");
        fclose(fp);
        return;
    }

    os_fseek(fp, (int64)offset, SEEK_SET);
    uint64 transmission_size = total_size - offset;
    asynclog_info("Sending file \"%s\" from offset %llu (%llu bytes remaining).", meta->filename, offset, transmission_size);

    #if defined(__linux__)
        if (socket_sendfile(c, fp, offset, transmission_size) < 0) {
            asynclog_warning("socket_sendfile failed.");
        }
    #else
        char buffer[BUFFER_CAPACITY];
        usize length;
        while ((length = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
            char *p = buffer;
            while (length > 0) {
                long n = socket_send(c, p, length, 0);
                if (n <= 0) {
                    asynclog_warning("Data transmission interrupted.");
                    goto end_download;
                }
                p += n;
                length -= n;
            }
        }
    #endif

    asynclog_info("File \"%s\" transmission completed.", meta->filename);

    #if !defined(__linux__)
    end_download:
    #endif
    fclose(fp);
    
    socket_shutdown(c);
}


void app_server_remove(Socket c, _TransportProtocolHeader *meta) {
    if (meta->filename[0] == '/' || meta->filename[0] == '\\') {
        asynclog_warning("refusal to process absolute path request: \"%s\".", meta->filename);
        socket_send(c, "\x1b[1;37;41m[Error]\x1b[0m unsafe path suspected to traverse boundary.", 65, 0);
        socket_shutdown(c);
        return;
    }

    char path[PATH_LENGTH_RESTRICTION];
    snprintf(path, sizeof(path), "./tcp-file-storage/%s", meta->filename);

    if (os_access(path) != 1) socket_send(c, "No such file or directory.", 27, 0);
    else {
        os_remove(path);
        char buffer[1024];
        int n = snprintf(buffer, sizeof(buffer), "Delete \"%s\" successfully.", meta->filename);
        if (n < 0 || n >= (int)sizeof(buffer)) n = sizeof(buffer) - 1;
        socket_send(c, buffer, n + 1, 0);
    }
    socket_shutdown(c);
}


void app_server_default(Socket c) {
    char response[] = "Default command";
    socket_send(c, response, strlen(response) + 1, 0);
    socket_shutdown(c);
}


void __app_server_list__(char *dir, char *name, bool folder, uint64 size, void *args) {
    (void)dir;

    int len;
    char unit[12];
    char buffer[PATH_LENGTH_RESTRICTION];
    Socket c = (Socket)uintptr(args);

    parse_unit_convert(size, unit, sizeof(unit));

    if (folder) len = snprintf(buffer, sizeof(buffer), "\x1b[1;36m%-12s%s\x1b[0m\n", "", name);
    else len = snprintf(buffer, sizeof(buffer), "%-12s%s\n", unit, name);
    if (len > 0) socket_send(c, buffer, len, 0);
}