#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "../protocol/data_link.h"
#include "packet.h"
#include "receive.h"
#include "send.h"

#define PATH_MAX 4096
#define DEFAULT_BYTES_PER_PACKET 100

bool option_flags[] = {false, false, false, false, false};

static int port = -1;
int bytes_per_packet = -1;
static char file_path[PATH_MAX] = "";
static char file_name[PATH_MAX] = "";
static device_role role;

/* static void print_bytes(char *buf, int size) {
    printf("size: %d\n", size);
    for (int i = 0; i < size; i++) {
        printf("%x ", buf[i]);
    }
    printf("\n\n");
} */

static void print_usage(char **argv) {
    printf("Usage: %s -p <port> -s|-r <filepath> [-n filename] [-b "
           "bytes_per_packet] [-v]\n",
           argv[0]);
}

void parse_options(int argc, char **argv) {
    int opt;
    bool got_role = false;
    while ((opt = getopt(argc, argv, ":p:s:r:n:b:v")) != -1) {
        switch (opt) {
            case 'p':
                errno = 0;
                port = atoi(optarg);
                if (errno != 0) {
                    perror("Port must be a valid number\n");
                    exit(-1);
                }
                option_flags[0] = true;
                break;
            case 's':
            case 'r':
                if (got_role) {
                    printf("Cannot receive and send at the same time\n");
                    exit(-1);
                }
                got_role = true;
                strncpy(file_path, optarg, PATH_MAX);
                role = opt == 's' ? TRANSMITTER : RECEIVER;
                option_flags[1] = true;
                break;
            case 'n':
                strncpy(file_name, optarg, PATH_MAX);
                option_flags[2] = true;
                break;
            case 'b':
                errno = 0;
                bytes_per_packet = atoi(optarg);
                if (errno != 0) {
                    perror("Bytes per packet must be a valid number\n");
                    exit(-1);
                }
                option_flags[3] = true;
                break;
            case 'v':
                option_flags[4] = true;
                break;
            case ':':
            case '?':
            default:
                print_usage(argv);
                exit(-1);
                break;
        }
    }
}

int assert_valid_options() {
    if (!option_flags[0]) {
        if (option_flags[4]) {
            fprintf(stderr, "Undefined port number\n");
        }
        return -1;
    }

    if (!option_flags[1]) {
        if (option_flags[4]) {
            fprintf(stderr, "No role file path pair provided\n");
        }
        return -1;
    }

    if (strlen(file_path) == 0) {
        if (option_flags[4]) {
            fprintf(stderr, "Empty file path %s\n", file_path);
        }
        return -1;
    }

    if (role == TRANSMITTER) {
        if (option_flags[3]) {
            if (bytes_per_packet <= 0 ||
                bytes_per_packet > MAX_DATA_PER_PACKET_SIZE) {
                if (option_flags[4]) {
                    fprintf(stderr, "Invalid bytes per packet\n");
                }
                return -1;
            }
        } else {
            bytes_per_packet = DEFAULT_BYTES_PER_PACKET;
            if (option_flags[4]) {
                printf(
                    "No bytes per packet value given, using default size %d\n",
                    DEFAULT_BYTES_PER_PACKET);
            }
        }
    }

    return 0;
}

int main(int argc, char **argv) {
    parse_options(argc, argv);
    if (assert_valid_options() == -1) {
        print_usage(argv);
        exit(1);
    }

    if (option_flags[4]) {
        printf("port: %d, role: %d, filepath: %s, filename: %s", port, role,
               file_path, file_name);
    }

    switch (role) {
        case TRANSMITTER:
            if (option_flags[4]) {
                printf(", bytesperpacket: %d\n", bytes_per_packet);
            }
            if (send_file(file_path, file_name, port, bytes_per_packet) != 0) {
                printf("No file sent\n");
                return -1;
            }
            break;
        case RECEIVER:
            if (option_flags[4]) {
                printf("\n");
            }

            if (receive_file(file_path, file_name, port) != 0) {
                printf("No file received\n");
                return -1;
            }
            break;
    }

    return 0;
}

/*
# Known Bugs
    Fails if transmitter launched first
    Penguin is missing bytes

*/
