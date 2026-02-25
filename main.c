#include "./utils/handle.h"


int main(int argc, char *argv[], char *envs[]) {
    if (argc < 2) {
        handle_usage_logo();
        handle_usage_help(argv[0], DEFAULT_PORT);
        return 1;
    }

    char exe_dir[256];
    char exe_path[256 + 128];
    os_getexec(exe_dir, sizeof(exe_dir));
    snprintf(exe_path, sizeof(exe_path), "%s/%s", exe_dir, ".tcp-config");

    if (strcmp(argv[1], "run") == 0) {
        if (argc == 2) {
            char local_ipv4[32];
            socket_ipv4(local_ipv4, sizeof(local_ipv4));
            handle_usage_logo();
            handle_usage_start(DEFAULT_PORT, local_ipv4);
            async_log_init(8, 32);
            async_log_setting(1);
            FILE *f = fopen("app.log", "a");
            async_log_config_write(f);
            handle_run_server(DEFAULT_PORT);
            async_log_exit(1);
            fclose(f);
            handle_usage_quit();
        }

        else if (argc == 3) {
            char *end;
            long port = strtol(argv[2], &end, 10);
            if (*end != '\0' || port <= 0) {
                printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31m\"%s\" is not a positive integer.\x1b[0m\n", argv[2]);
                return 1;
            } else {
                char local_ipv4[32];
                socket_ipv4(local_ipv4, sizeof(local_ipv4));
                handle_usage_logo();
                handle_usage_start(port, local_ipv4);
                async_log_init(8, 32);
                async_log_setting(1);
                FILE *f = fopen("app.log", "a");
                async_log_config_write(f);
                handle_run_server(port);
                async_log_exit(1);
                fclose(f);
                handle_usage_quit();
            }
        }

        else {
            printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31mat most one additional parameter.\x1b[0m\n");
            printf(">>> \x1b[1;32m%s run [%d]\x1b[0m\n", argv[0], DEFAULT_PORT);
            return 1;
        }
    }

    else if (strcmp(argv[1], "config") == 0) {
        char host_ipv4[16];
        int host_port;

        if (argc == 2) {
            if (os_access(exe_path) == 1) {
                if (handle_read_config(host_ipv4, &host_port) == 0) {
                    printf("IPv4: %s\nPort: %d\n", host_ipv4, host_port);
                } else {
                    printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31minvalid \".tcp-config\" file format in \"%s\".\x1b[0m\n", exe_dir);
                    printf("\x1b[1;32m[Correct Format]\x1b[0m\n127.0.0.1\n31415\n");
                    return 1;
                }
            } else {
                printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31mno configuration file in \"%s\".\x1b[0m\n", exe_dir);
                printf(">>> \x1b[1;32m%s config [ipv4:port]\x1b[0m\n", argv[0]);
                return 1;
            }
        }

        else if (argc == 3) {
            int result = handle_parse_config(argv[2], host_ipv4, &host_port);
            if (result == 1) printf("\x1b[1;37;43m[Warning]\x1b[0m \x1b[1;33minvalid socket (default configuration).\x1b[0m\n");
            handle_write_config(host_ipv4, host_port);
            printf("\x1b[1;37;42m[Success]\x1b[0m \x1b[1;32m\".tcp-config\" is written to \"%s\".\x1b[0m\n", exe_dir);
        }

        else {
            printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31mconfiguration displaying or parsing failed.\x1b[0m\n");
            printf(">>> \x1b[1;32m%s config [ipv4:port]\x1b[0m\n", argv[0]);
            return 1;
        }
    }

    else if (strcmp(argv[1], "send") == 0) {
        if (argc == 2 || argc > 3) {
            printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31mplease input the file path.\x1b[0m\n");
            printf(">>> \x1b[1;32m%s send [filepath]\x1b[0m\n", argv[0]);
            return 1;
        } else {
            if (os_access(argv[2]) != 1) {
                printf("\x1b[1;37;43m[Warning]\x1b[0m \x1b[1;33mfile \"%s\" does not exist.\x1b[0m\n", argv[2]);
                return 1;
            }

            if (os_access(exe_path) != 1) {
                char local_ipv4[32];
                socket_ipv4(local_ipv4, sizeof(local_ipv4));
                printf("\x1b[1;37;43m[Warning]\x1b[0m \x1b[1;33mno \".tcp-config\" file in \"%s\".\x1b[0m\n", exe_dir);
                printf("You can configure the remote server default socket in client:\n>>> \x1b[1;32m%s config %s:%d\x1b[0m\n", argv[0], local_ipv4, DEFAULT_PORT);
                handle_run_client(argv[2], local_ipv4, DEFAULT_PORT);
            } else {
                char host_ipv4[32];
                int host_port;
                if (handle_read_config(host_ipv4, &host_port) == 0) {
                    handle_run_client(argv[2], host_ipv4, host_port);
                } else {
                    printf("\x1b[1;37;41m[Error]\x1b[0m \x1b[1;31minvalid \".tcp-config\" file format in \"%s\".\x1b[0m\n", exe_dir);
                    printf("\x1b[1;32m[Correct Format]\x1b[0m\n127.0.0.1\n31415\n");
                    return 1;
                }
            }
        }
    }

    else {
        handle_usage_logo();
        handle_usage_help(argv[0], DEFAULT_PORT);
        return 1;
    }

    return 0;
}
