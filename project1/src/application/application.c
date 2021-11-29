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

static struct {
    bool port;
    bool role_path;
    bool name;
    bool bytes_per_packet;
    bool verbose;
} options = {false, false, false, false, false};

// BESIDES DE PROGRESS BAR, CAN WE PUT "RECEIVING X... RECEIVED X REALLY
// BEAUTIFULLY?" IF ALL FUNCTIONING WELL REMOVE DEBUG PRINTS BJ

static int port = -1;
int bytes_per_packet = -1;
static char file_path[PATH_MAX] = "";
static char file_name[PATH_MAX] = "";
static device_role role;

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
                options.port = true;
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
                options.role_path = true;
                break;
            case 'n':
                strncpy(file_name, optarg, PATH_MAX);
                options.name = true;
                break;
            case 'b':
                errno = 0;
                bytes_per_packet = atoi(optarg);
                if (errno != 0) {
                    perror("Bytes per packet must be a valid number\n");
                    exit(-1);
                }
                options.bytes_per_packet = true;
                break;
            case 'v':
                options.verbose = true;
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
    if (!options.port) {
        if (options.verbose) {
            fprintf(stderr, "Undefined port number\n");
        }
        return -1;
    }

    if (!options.role_path) {
        if (options.verbose) {
            fprintf(stderr, "No role file path pair provided\n");
        }
        return -1;
    }

    if (strlen(file_path) == 0) {
        if (options.verbose) {
            fprintf(stderr, "Empty file path %s\n", file_path);
        }
        return -1;
    }

    if (role == TRANSMITTER) {
        if (options.bytes_per_packet) {
            if (bytes_per_packet <= 0 ||
                bytes_per_packet > MAX_DATA_PER_PACKET_SIZE) {
                if (options.verbose) {
                    fprintf(stderr, "Invalid bytes per packet\n");
                }
                return -1;
            }
        } else {
            bytes_per_packet = DEFAULT_BYTES_PER_PACKET;
            if (options.verbose) {
                printf(
                    "No bytes per packet value given, using default size %d\n",
                    DEFAULT_BYTES_PER_PACKET);
            }
        }
    } else {
        if (options.bytes_per_packet) {
            fprintf(
                stderr,
                "Application can't specify bytes per packet when receiving.\n");
            return -1;
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

    if (options.verbose) {
        printf("port: %d, role: %d, filepath: %s, filename: %s", port, role,
               file_path, file_name);
    }

    switch (role) {
        case TRANSMITTER:
            if (options.verbose) {
                printf(", bytesperpacket: %d\n", bytes_per_packet);
            }
            if (send_file(file_path, file_name, port, bytes_per_packet) != 0) {
                printf("No file sent\n");
                return -1;
            }
            break;
        case RECEIVER:
            if (options.verbose) {
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
