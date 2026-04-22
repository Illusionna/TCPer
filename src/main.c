#include "./utils/app.h"
#include "./utils/usage.h"
#include "./utils/parse.h"


static void __thread_lock_callback__(int lock, void *ctx) {
    Mutex *mutex = (Mutex *)ctx;
    if (lock) mutex_lock(mutex);
    else mutex_unlock(mutex);
}


int main(int argc, char *argv[], char *envs[]) {
    char username[256];
    char ipv4[32];
    char app_dir[PATH_LENGTH_RESTRICTION];
    char config_path[PATH_LENGTH_RESTRICTION];

    parse_username(username, envs);
    os_getexec(app_dir, sizeof(app_dir));
    snprintf(config_path, sizeof(config_path), "%s/%s", app_dir, ".tcp-server-config");

    if (argc < 2) {
        usage_logo();
        usage_help(username, argv[0], DEFAULT_SERVICE_PORT);
        return 1;
    }

    if (strcmp(argv[1], "run") == 0) {
        if (argc == 2) {
            socket_ipv4(ipv4, sizeof(ipv4));
            usage_logo();
            app_start_tips(ipv4, DEFAULT_SERVICE_PORT);

            Mutex mutex;
            mutex_create(&mutex, 1);
            async_log_config_thread_lock(__thread_lock_callback__, &mutex);
            async_log_init(8, 32);
            FILE *f = fopen("app.log", "a");
            if (f) {
                async_log_config_write(f);
                async_log_setting(1);
            } else {
                async_log_setting(0);
                fprintf(stderr, "\x1b[1;37;43m[Warning]\x1b[0m failed to open \"app.log\", fallback to stderr.\n");
            }
            app_run_service(DEFAULT_SERVICE_PORT);
            async_log_exit(1);
            mutex_destroy(&mutex);

            if (f) fclose(f);
            app_quit_tips();
        }

        else if (argc == 3) {
            int port = parse_port(argv[2]);
            if (port == 0) {
                printf("\x1b[1;37;41m[Error]\x1b[0m \"%s\" is not an integer within the range of 1 ~ 65535.\n", argv[2]);
                return 1;
            }

            socket_ipv4(ipv4, sizeof(ipv4));
            usage_logo();
            app_start_tips(ipv4, port);

            Mutex mutex;
            mutex_create(&mutex, 1);
            async_log_config_thread_lock(__thread_lock_callback__, &mutex);
            async_log_init(8, 32);

            FILE *f = fopen("app.log", "a");
            if (f) {
                async_log_config_write(f);
                async_log_setting(1);
            } else {
                async_log_setting(0);
                fprintf(stderr, "\x1b[1;37;43m[Warning]\x1b[0m failed to open \"app.log\", fallback to stderr.\n");
            }
            app_run_service(port);
            async_log_exit(1);
            mutex_destroy(&mutex);

            if (f) fclose(f);
            app_quit_tips();
        }

        else {
            printf("\x1b[1;37;41m[Error]\x1b[0m at most one additional parameter.\n");
            printf("\n * \x1b[90m(server)\x1b[0m %s run [port]\n\t>>> \x1b[1;32m%s run %d\x1b[0m\n", argv[0], argv[0], DEFAULT_SERVICE_PORT);
            return 1;
        }
    }

    else if (strcmp(argv[1], "config") == 0) {
        char host_ipv4[32];
        int host_port;
        char proxy_ipv4[32];
        int proxy_port;

        if (argc == 2) {
            if (os_access(config_path) != 1) {
                printf("\x1b[1;37;41m[Error]\x1b[0m no \"%s\" configuration file exists.\n", config_path);
                printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=8.8.8.8:%d --proxy=127.0.0.1:7890\x1b[0m\n", argv[0], argv[0], DEFAULT_SERVICE_PORT);
                return 1;
            }
            if (parse_config_file(config_path, host_ipv4, &host_port, proxy_ipv4, &proxy_port) == 1) {
                printf("\x1b[1;37;41m[Error]\x1b[0m failed to read file \"%s\".\n", config_path);
                return 1;
            }
            printf("host ipv4 = %s\nhost port = %d\nproxy ipv4 = %s\nproxy port = %d\n", host_ipv4, host_port, proxy_ipv4, proxy_port);
        }

        else if (argc == 3 || argc == 4) {
            char *arg1 = argv[2];
            char *arg2 = (argc == 4) ? argv[3] : NULL;
            if (parse_config_cmd(arg1, arg2, host_ipv4, &host_port, proxy_ipv4, &proxy_port) == 1) {
                printf("\x1b[1;37;43m[Warning]\x1b[0m invalid socket configuration.\n");
                printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=8.8.8.8:%d --proxy=127.0.0.1:7890\x1b[0m\n", argv[0], argv[0], DEFAULT_SERVICE_PORT);
                return 1;
            }
            if (parse_config_write(config_path, host_ipv4, host_port, proxy_ipv4, proxy_port)) {
                printf("\x1b[1;37;41m[Error]\x1b[0m configuration write to \"%s\" file failed.\n", config_path);
                return 1;
            }
            printf("\x1b[1;37;42m[Success]\x1b[0m \".tcp-server-config\" is written to \"%s\".\n", app_dir);
        }

        else {
            printf("\x1b[1;37;41m[Error]\x1b[0m configuration failed.\n");
            printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=8.8.8.8:%d --proxy=127.0.0.1:7890\x1b[0m\n", argv[0], argv[0], DEFAULT_SERVICE_PORT);
            return 1;
        }
    }

    else if (strcmp(argv[1], "send") == 0) {
        if (argc == 2 || argc > 3) {
            printf("\x1b[1;37;41m[Error]\x1b[0m no such file or directory.\n");
            printf("\n * \x1b[90m(client)\x1b[0m %s send <path>\n\t>>> \x1b[1;32m%s send \"A:\\Illusionna\\Desktop\\My PDF\\demo.pdf\"\x1b[0m\n", argv[0], argv[0]);
            return 1;
        }

        else {
            char host_ipv4[32];
            int host_port;
            char proxy_ipv4[32];
            int proxy_port;

            if (os_access(argv[2]) != 1) {
                printf("\x1b[1;37;43m[Warning]\x1b[0m no \"%s\" such file or directory.\n", argv[2]);
                return 1;
            }

            socket_ipv4(ipv4, sizeof(ipv4));

            if (os_access(config_path) != 1) {
                printf("\x1b[1;37;43m[Warning]\x1b[0m no \"%s\" configuration file exists.", config_path);
                printf("\n\nHi %s, attention! You can configure the remote server socket in client:\n", username);
                printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=%s:%d --proxy=127.0.0.1:7890\x1b[0m\n\n", argv[0], argv[0], ipv4, DEFAULT_SERVICE_PORT);
                app_client_send(argv[2], ipv4, DEFAULT_SERVICE_PORT, NULL, 0);
            }

            else {
                if (parse_config_file(config_path, host_ipv4, &host_port, proxy_ipv4, &proxy_port) == 1) {
                    printf("\x1b[1;37;41m[Error]\x1b[0m failed to read file \"%s\".\n", config_path);
                    return 1;
                }

                if (!socket_valid_ipv4(host_ipv4) || host_port <= 0 || host_port > 65535) {
                    printf("\x1b[1;37;41m[Error]\x1b[0m invalid host socket config in \"%s\".\n", config_path);
                    printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=%s:%d --proxy=127.0.0.1:7890\x1b[0m\n", argv[0], argv[0], ipv4, DEFAULT_SERVICE_PORT);
                    return 1;
                }

                if (strcmp(proxy_ipv4, "") != 0) {
                    if (!socket_valid_ipv4(proxy_ipv4) || proxy_port <= 0 || proxy_port > 65535) {
                        printf("\x1b[1;37;41m[Error]\x1b[0m invalid proxy socket config in \"%s\".\n", config_path);
                        printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=%s:%d --proxy=127.0.0.1:7890\x1b[0m\n", argv[0], argv[0], ipv4, DEFAULT_SERVICE_PORT);
                        return 1;
                    }
                    app_client_send(argv[2], host_ipv4, host_port, proxy_ipv4, proxy_port);
                } else app_client_send(argv[2], host_ipv4, host_port, NULL, 0);
            }
        }
    }

    else if (strcmp(argv[1], "list") == 0) {
        if (argc == 2 || argc > 3) {
            printf("\x1b[1;37;41m[Error]\x1b[0m no such file or directory.\n");
            printf("\n * \x1b[90m(client)\x1b[0m %s list <dir>\n\t>>> \x1b[1;32m%s list .\x1b[0m\n", argv[0], argv[0]);
            return 1;
        }

        else {
            char host_ipv4[32];
            int host_port;
            char proxy_ipv4[32];
            int proxy_port;

            socket_ipv4(ipv4, sizeof(ipv4));

            if (os_access(config_path) != 1) {
                printf("\x1b[1;37;43m[Warning]\x1b[0m no \"%s\" configuration file exists.", config_path);
                printf("\n\nHi %s, attention! You can configure the remote server socket in client:\n", username);
                printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=%s:%d --proxy=127.0.0.1:7890\x1b[0m\n\n", argv[0], argv[0], ipv4, DEFAULT_SERVICE_PORT);
                app_client_list(argv[2], ipv4, DEFAULT_SERVICE_PORT);
            }

            else {
                if (parse_config_file(config_path, host_ipv4, &host_port, proxy_ipv4, &proxy_port) != 0) {
                    printf("\x1b[1;37;41m[Error]\x1b[0m failed to read file \"%s\".\n", config_path);
                    return 1;
                }
                if (!socket_valid_ipv4(host_ipv4) || host_port <= 0 || host_port > 65535) {
                    printf("\x1b[1;37;41m[Error]\x1b[0m invalid host socket config in \"%s\".\n", config_path);
                    printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=%s:%d --proxy=127.0.0.1:7890\x1b[0m\n", argv[0], argv[0], ipv4, DEFAULT_SERVICE_PORT);
                    return 1;
                }
                app_client_list(argv[2], host_ipv4, host_port);
            }
        }
    }

    else if (strcmp(argv[1], "download") == 0) {
        if (argc == 2 || argc > 3) {
            printf("\x1b[1;37;41m[Error]\x1b[0m no such file or directory.\n");
            printf("\n * \x1b[90m(client)\x1b[0m %s download <path>\n\t>>> \x1b[1;32m%s download ./algorithms/LLL.png\x1b[0m\n", argv[0], argv[0]);
            return 1;
        }

        else {
            char host_ipv4[32];
            int host_port;
            char proxy_ipv4[32];
            int proxy_port;

            socket_ipv4(ipv4, sizeof(ipv4));

            if (os_access(config_path) != 1) {
                printf("\x1b[1;37;43m[Warning]\x1b[0m no \"%s\" configuration file exists.", config_path);
                printf("\n\nHi %s, attention! You can configure the remote server socket in client:\n", username);
                printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=%s:%d --proxy=127.0.0.1:7890\x1b[0m\n\n", argv[0], argv[0], ipv4, DEFAULT_SERVICE_PORT);
                app_client_download(argv[2], ".", ipv4, DEFAULT_SERVICE_PORT, NULL, 0);
            }

            else {
                if (parse_config_file(config_path, host_ipv4, &host_port, proxy_ipv4, &proxy_port) != 0) {
                    printf("\x1b[1;37;41m[Error]\x1b[0m failed to read file \"%s\".\n", config_path);
                    return 1;
                }

                if (!socket_valid_ipv4(host_ipv4) || host_port <= 0 || host_port > 65535) {
                    printf("\x1b[1;37;41m[Error]\x1b[0m invalid host socket config in \"%s\".\n", config_path);
                    printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=%s:%d --proxy=127.0.0.1:7890\x1b[0m\n", argv[0], argv[0], ipv4, DEFAULT_SERVICE_PORT);
                    return 1;
                }

                if (strcmp(proxy_ipv4, "") != 0) {
                    if (!socket_valid_ipv4(proxy_ipv4) || proxy_port <= 0 || proxy_port > 65535) {
                        printf("\x1b[1;37;41m[Error]\x1b[0m invalid proxy socket config in \"%s\".\n", config_path);
                        printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=%s:%d --proxy=127.0.0.1:7890\x1b[0m\n", argv[0], argv[0], ipv4, DEFAULT_SERVICE_PORT);
                        return 1;
                    }
                    app_client_download(argv[2], ".", host_ipv4, host_port, proxy_ipv4, proxy_port);
                } else app_client_download(argv[2], ".", host_ipv4, host_port, NULL, 0);
            }
        }
    }

    else if (strcmp(argv[1], "remove") == 0) {
        if (argc == 2 || argc > 3) {
            printf("\x1b[1;37;41m[Error]\x1b[0m no such file or directory.\n");
            printf("\n ! \x1b[90m(client)\x1b[0m %s remove <path>\n\t>>> \x1b[1;31m%s remove storage/pkg/bin\x1b[0m\n", argv[0], argv[0]);
            return 1;
        }

        else {
            char host_ipv4[32];
            int host_port;
            char proxy_ipv4[32];
            int proxy_port;

            socket_ipv4(ipv4, sizeof(ipv4));

            if (os_access(config_path) != 1) {
                printf("\x1b[1;37;43m[Warning]\x1b[0m no \"%s\" configuration file exists.", config_path);
                printf("\n\nHi %s, attention! You can configure the remote server socket in client:\n", username);
                printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=%s:%d --proxy=127.0.0.1:7890\x1b[0m\n\n", argv[0], argv[0], ipv4, DEFAULT_SERVICE_PORT);
                app_client_remove(argv[2], ipv4, DEFAULT_SERVICE_PORT);
            }

            else {
                if (parse_config_file(config_path, host_ipv4, &host_port, proxy_ipv4, &proxy_port) != 0) {
                    printf("\x1b[1;37;41m[Error]\x1b[0m failed to read file \"%s\".\n", config_path);
                    return 1;
                }
                if (!socket_valid_ipv4(host_ipv4) || host_port <= 0 || host_port > 65535) {
                    printf("\x1b[1;37;41m[Error]\x1b[0m invalid host socket config in \"%s\".\n", config_path);
                    printf("\n * \x1b[90m(client)\x1b[0m %s config [host] [proxy]\n\t>>> \x1b[1;32m%s config --host=%s:%d --proxy=127.0.0.1:7890\x1b[0m\n", argv[0], argv[0], ipv4, DEFAULT_SERVICE_PORT);
                    return 1;
                }
                app_client_remove(argv[2], host_ipv4, host_port);
            }
        }
    }

    else {
        usage_logo();
        usage_help(username, argv[0], DEFAULT_SERVICE_PORT);
        return 1;
    }

    return 0;
}
