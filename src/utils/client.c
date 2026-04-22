#include "app.h"


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
    meta.command = APP_COMMAND_SEND;
    strncpy(meta.md5, hash, sizeof(meta.md5) - 1);
    meta.md5[sizeof(meta.md5) - 1] = '\0';
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

    byte nonce[16];
    if (socket_recv(s, (char *)nonce, 16, 0) != 16) {
        printf("\x1b[1;37;41m[Error]\x1b[0m receiving challenge nonce failed.\n");
        goto release_client;
    }

    parse_rc4((byte *)PASSWORD_TOKEN, strlen(PASSWORD_TOKEN), nonce, 16);

    if (socket_send(s, (char *)nonce, 16, 0) != 16) {
        printf("\x1b[1;37;41m[Error]\x1b[0m sending challenge response failed.\n");
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
            parse_rc4((byte *)PASSWORD_TOKEN, strlen(PASSWORD_TOKEN), (byte *)buffer, length);

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


void app_client_list(char *dir, char *host_ipv4, int host_port) {
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

    int send_buffer_size = BUFFER_CAPACITY;
    if (socket_setopt(s, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size)) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m socket buffer setting failed.\n");
        goto release_client_list;
    }

    if (socket_setopt_timeout(s, 1, 7.0) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m socket receiver timeout setting failed.\n");
        goto release_client_list;
    }

    if (socket_setopt(s, IPPROTO_TCP, TCP_NODELAY, NULL, 0) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m disable Nagle algorithm failed.\n");
        goto release_client_list;
    }

    struct sockaddr_in direct_server;
    memset(&direct_server, 0, sizeof(direct_server));
    socket_config(&direct_server, AF_INET, host_ipv4, host_port);

    if (socket_connect_timeout(s, &direct_server, sizeof(direct_server), 3.7) == SOCKET_INVALID) { 
        printf("\x1b[1;37;41m[Error]\x1b[0m is the server \"%s:%d\" running?\n", host_ipv4, host_port);
        goto release_client_list;
    }

    _TransportProtocolHeader meta;
    memset(&meta, 0, sizeof(meta));
    meta.command = APP_COMMAND_LIST;
    strncpy(meta.token, PASSWORD_TOKEN, sizeof(meta.token) - 1);
    meta.token[sizeof(meta.token) - 1] = '\0';
    strncpy(meta.filename, dir, sizeof(meta.filename) - 1);
    meta.filename[sizeof(meta.filename) - 1] = '\0';

    if (socket_send(s, (char *)&meta, sizeof(meta), 0) != sizeof(meta)) {
        printf("\x1b[1;37;41m[Error]\x1b[0m sending file meta header failed.\n");
        goto release_client_list;
    }

    char status;
    if (socket_recv(s, &status, 1, 0) != 1) {
        printf("\x1b[1;37;41m[Error]\x1b[0m receiving handshake status failed.\n");
        goto release_client_list;
    }

    if (status != HANDSHAKE_OK) {
        if (status == HANDSHAKE_ERROR_TOKEN) printf("\x1b[1;37;41m[Error]\x1b[0m invalid authentication password token!\n");
        else if (status == HANDSHAKE_ERROR_PATH) printf("\x1b[1;37;41m[Error]\x1b[0m unsafe path suspected to traverse boundary.\n");
        else printf("\x1b[1;37;41m[Error]\x1b[0m unknown handshake error (0x%02x).\n", status);
        goto release_client_list;
    }

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

    release_client_list:
        socket_close(s);
        socket_destroy();
}


void app_client_download(char *remote_file, char *save_path, char *host_ipv4, int host_port, char *proxy_ipv4, int proxy_port) {
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

    int recv_buffer_size = BUFFER_CAPACITY;
    if (socket_setopt(s, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, sizeof(recv_buffer_size)) == SOCKET_INVALID) {
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

    // ==========================================
    // 1. 网络连接建立与代理协商 (与 Send 相同)
    // ==========================================
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
        if (socket_send(s, authentication_request, 3, 0) != 3) goto release_client;

        char authentication_response[2];
        if (socket_recv(s, authentication_response, 2, 0) != 2 || authentication_response[0] != 0x05 || authentication_response[1] != 0x00) {
            printf("\x1b[1;37;41m[Error]\x1b[0m proxy authentication rejected.\n");
            goto release_client;
        }

        char vpn_request[10] = {0x05, 0x01, 0x00, 0x01};
        struct in_addr remote_ipv4;
        inet_pton(AF_INET, host_ipv4, &remote_ipv4);
        memcpy(&vpn_request[4], &remote_ipv4.s_addr, 4);
        unsigned short remote_port = socket_htons(host_port);
        memcpy(&vpn_request[8], &remote_port, 2);

        if (socket_send(s, vpn_request, 10, 0) != 10) goto release_client;

        char vpn_response[260];
        if (socket_recv(s, vpn_response, 4, 0) != 4 || vpn_response[1] != 0x00) {
            printf("\x1b[1;37;41m[Error]\x1b[0m proxy route to server failed.\n");
            goto release_client;
        }

        int remain = (vpn_response[3] == 0x01) ? 6 : (vpn_response[3] == 0x04) ? 18 : 0;
        if (vpn_response[3] == 0x03) {
            if (socket_recv(s, vpn_response, 1, 0) != 1) goto release_client;
            remain = (unsigned char)vpn_response[0] + 2;
        }
        if (remain > 0 && socket_recv(s, vpn_response, remain, 0) != remain) goto release_client;

        printf("\x1b[1;37;42m[INFO]\x1b[0m proxy Socks5 VPN Tunnel established.\n");
    }

    _TransportProtocolHeader meta;
    memset(&meta, 0, sizeof(meta));
    meta.command = APP_COMMAND_DOWNLOAD;
    strncpy(meta.token, PASSWORD_TOKEN, sizeof(meta.token) - 1);
    strncpy(meta.filename, remote_file, sizeof(meta.filename) - 1);

    if (socket_send(s, (char *)&meta, sizeof(meta), 0) != sizeof(meta)) {
        printf("\x1b[1;37;41m[Error]\x1b[0m sending download header failed.\n");
        goto release_client;
    }

    char status;
    if (socket_recv(s, &status, 1, 0) != 1) {
        printf("\x1b[1;37;41m[Error]\x1b[0m receiving handshake status failed.\n");
        goto release_client;
    }

    if (status != HANDSHAKE_OK) {
        if (status == HANDSHAKE_ERROR_TOKEN) printf("\x1b[1;37;41m[Error]\x1b[0m invalid token!\n");
        else if (status == HANDSHAKE_ERROR_PATH) printf("\x1b[1;37;41m[Error]\x1b[0m unsafe path suspected.\n");
        else printf("\x1b[1;37;41m[Error]\x1b[0m unknown handshake error.\n");
        goto release_client;
    }

    uint64 network_total_size;
    if (socket_recv(s, (char *)&network_total_size, sizeof(network_total_size), 0) != sizeof(network_total_size)) {
        printf("\x1b[1;37;41m[Error]\x1b[0m receiving file size failed.\n");
        goto release_client;
    }

    uint64 total_size = socket_ntohll(network_total_size);
    if (total_size == 0) {
        printf("\x1b[1;37;41m[Error]\x1b[0m remote file \"%s\" not found or inaccessible.\n", remote_file);
        goto release_client;
    }

    char hash[33] = {0};
    if (socket_recv(s, hash, 32, 0) != 32) {
        printf("\x1b[1;37;41m[Error]\x1b[0m receiving file MD5 failed.\n");
        goto release_client;
    }

    if (!save_path || save_path[0] == '\0') {
    printf("[Error] empty save path.\n");
    goto release_client;
}

    char actual_save_path[PATH_LENGTH_RESTRICTION];
    usize sp_len = strlen(save_path);
    if (strcmp(save_path, ".") == 0 || save_path[sp_len - 1] == '/' || save_path[sp_len - 1] == '\\') {
        snprintf(actual_save_path, sizeof(actual_save_path), "%s/%s",
        strcmp(save_path, ".") == 0 ? "." : save_path,
        os_basename(remote_file));
    } else snprintf(actual_save_path, sizeof(actual_save_path), "%s", save_path);

    char temp_file[PATH_LENGTH_RESTRICTION * 2];
    snprintf(temp_file, sizeof(temp_file), "%s_%s.downloading", actual_save_path, hash);

    int64 finished_size = os_filesize(actual_save_path);
    int64 temp_size = os_filesize(temp_file);

    if (finished_size != -1 && (uint64)finished_size >= total_size) {
        printf("\x1b[1;37;42m[Success]\x1b[0m file \"%s\" already downloaded.\n", os_basename(actual_save_path));
        uint64 off = socket_htonll(total_size);
        socket_send(s, (char *)&off, sizeof(off), 0);
        goto release_client;
    }

    uint64 offset = (temp_size != -1) ? (uint64)temp_size : 0;
    uint64 network_offset = socket_htonll(offset);

    if (socket_send(s, (char *)&network_offset, sizeof(network_offset), 0) != sizeof(network_offset)) {
        printf("\x1b[1;37;41m[Error]\x1b[0m sending offset failed.\n");
        goto release_client;
    }

    if (offset > 0 && offset < total_size) {
        printf("\x1b[1;37;42m[INFO]\x1b[0m resumable download from \x1b[1;32m[%llu]\x1b[0m to \x1b[1;32m[%llu]\x1b[0m bytes.\n", offset, total_size);
    } else {
        printf("\x1b[1;37;42m[INFO]\x1b[0m awaiting data...\n");
    }

    fp = fopen(temp_file, (os_access(temp_file) == 1) ? "r+b" : "wb");
    if (!fp) {
        printf("\x1b[1;37;41m[Error]\x1b[0m cannot open temporary file \"%s\" for writing.\n", temp_file);
        goto release_client;
    }
    os_fseek(fp, (int64)offset, SEEK_SET);

    uint64 transmission_size = total_size - offset;
    uint64 received_bytes = 0;
    char buffer[BUFFER_CAPACITY];
    bool transfer_ok = True;

    int step = BUFFER_CAPACITY;
    char description[32] = "0.000 B/s";
    char speed_buffer[16];
    uint64 last_bytes = 0;
    double last_time = os_time();

    while (received_bytes < transmission_size) {
        long n = socket_recv(s, buffer, 
            (transmission_size - received_bytes > sizeof(buffer)) ? sizeof(buffer) : transmission_size - received_bytes, 0);
        
        if (n <= 0) {
            printf("\n\x1b[1;37;41m[Error]\x1b[0m network disconnected or timeout during data streaming.\n");
            transfer_ok = False;
            break;
        }

        if (fwrite(buffer, 1, n, fp) != (usize)n) {
            printf("\n\x1b[1;37;41m[Error]\x1b[0m disk write failed. Out of space?\n");
            transfer_ok = False;
            break;
        }

        received_bytes += n;

        double current_time = os_time();
        double interval = current_time - last_time;
        if (interval >= 0.2) {
            double speed = (double)(received_bytes - last_bytes) / interval;
            parse_unit_convert((uint64)speed, speed_buffer, sizeof(speed_buffer));
            snprintf(description, sizeof(description), "%s/s", speed_buffer);
            last_time = current_time;
            last_bytes = received_bytes;
        }
        usage_progress_bar(received_bytes, transmission_size, step, description);
    }
    
    fflush(fp);
    fclose(fp);
    fp = NULL;

    if (transfer_ok && (offset + received_bytes >= total_size)) {
        printf("\n\x1b[1;37;42m[INFO]\x1b[0m verifying MD5 checksum...\n");
        
        FILE *vfp = fopen(temp_file, "rb");
        if (vfp) {
            char local_hash[33];
            md5_file(vfp, local_hash);
            fclose(vfp);

            if (strcmp(local_hash, hash) == 0) {
                if (rename(temp_file, actual_save_path) == 0) {
                    printf("\x1b[1;37;42m[Success]\x1b[0m downloaded successfully and saved to \"%s\".\n", actual_save_path);
                } else {
                    printf("\x1b[1;37;41m[Error]\x1b[0m failed to rename temporary file to \"%s\".\n", actual_save_path);
                }
            } else {
                printf("\x1b[1;37;41m[Error]\x1b[0m MD5 mismatch! The file is corrupted.\n");
                printf(" - Expected: %s\n - Actual:   %s\n", hash, local_hash);
                remove(temp_file); 
            }
        }
    }

    release_client:
        if (fp) fclose(fp);
        if (s != SOCKET_INVALID) {
            socket_close(s);
            socket_destroy();
        }
}


void app_client_remove(char *path, char *host_ipv4, int host_port) {
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

    int send_buffer_size = BUFFER_CAPACITY;
    if (socket_setopt(s, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size)) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m socket buffer setting failed.\n");
        goto release_client_remove;
    }

    if (socket_setopt_timeout(s, 1, 7.0) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m socket receiver timeout setting failed.\n");
        goto release_client_remove;
    }

    if (socket_setopt(s, IPPROTO_TCP, TCP_NODELAY, NULL, 0) == SOCKET_INVALID) {
        printf("\x1b[1;37;41m[Error]\x1b[0m disable Nagle algorithm failed.\n");
        goto release_client_remove;
    }

    struct sockaddr_in direct_server;
    memset(&direct_server, 0, sizeof(direct_server));
    socket_config(&direct_server, AF_INET, host_ipv4, host_port);

    if (socket_connect_timeout(s, &direct_server, sizeof(direct_server), 3.7) == SOCKET_INVALID) { 
        printf("\x1b[1;37;41m[Error]\x1b[0m is the server \"%s:%d\" running?\n", host_ipv4, host_port);
        goto release_client_remove;
    }

    _TransportProtocolHeader meta;
    memset(&meta, 0, sizeof(meta));
    meta.command = APP_COMMAND_REMOVE;
    strncpy(meta.token, PASSWORD_TOKEN, sizeof(meta.token) - 1);
    meta.token[sizeof(meta.token) - 1] = '\0';
    strncpy(meta.filename, path, sizeof(meta.filename) - 1);
    meta.filename[sizeof(meta.filename) - 1] = '\0';

    if (socket_send(s, (char *)&meta, sizeof(meta), 0) != sizeof(meta)) {
        printf("\x1b[1;37;41m[Error]\x1b[0m sending file meta header failed.\n");
        goto release_client_remove;
    }

    char status;
    if (socket_recv(s, &status, 1, 0) != 1) {
        printf("\x1b[1;37;41m[Error]\x1b[0m receiving handshake status failed.\n");
        goto release_client_remove;
    }

    if (status != HANDSHAKE_OK) {
        if (status == HANDSHAKE_ERROR_TOKEN) printf("\x1b[1;37;41m[Error]\x1b[0m invalid authentication password token!\n");
        else if (status == HANDSHAKE_ERROR_PATH) printf("\x1b[1;37;41m[Error]\x1b[0m unsafe path suspected to traverse boundary.\n");
        else printf("\x1b[1;37;41m[Error]\x1b[0m unknown handshake error (0x%02x).\n", status);
        goto release_client_remove;
    }

    char response[1024];
    usize accumulation = 0;
    while (accumulation < sizeof(response) - 1) {
        long n = socket_recv(s, response + accumulation, sizeof(response) - accumulation - 1, 0);
        if (n <= 0) break;
        char *zero = memchr(response + accumulation, '\0', n);
        accumulation = accumulation + n;
        if (zero) {
            accumulation = (usize)(zero - response);
            break;
        }
    }
    response[accumulation] = '\0';

    if (accumulation == 0) printf("\x1b[1;37;43m[Warning]\x1b[0m receiving socket data failed.\n");
    else printf("%s\n", response);

    release_client_remove:
        socket_close(s);
        socket_destroy();
}