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
int bytes_per_packet = DEFAULT_BYTES_PER_PACKET;
static char file_name[MAX_FILENAME_LENGTH] = "";
static char file_path[PATH_MAX] = "";
static device_role role;

static void print_usage(char **argv) {
    printf("Usage: %s [-v] -p <port> -s <filepath> -r <outdirectory> [-n "
           "filename] "
           "[-b "
           "bytesperpacket]\n",
           argv[0]);
}

static int parse_options(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, ":p:s:r:n:b:v")) != -1) {
        switch (opt) {
            case 'p':
                errno = 0;
                port = atoi(optarg);
                if (errno != 0) {
                    perror("Port must be a valid number\n");
                    return -1;
                }
                options.port = true;
                break;
            case 's':
            case 'r':
                if (options.role_path) {
                    printf("Cannot receive and send at the same time\n");
                    return -1;
                }
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
                    return -1;
                }
                options.bytes_per_packet = true;
                break;
            case 'v':
                options.verbose = true;
                break;
            default:
                return -1;
        }
    }
    return 0;
}

int assert_valid_options() {
    /* Validate port */
    if (!options.port) {
        if (options.verbose) {
            fprintf(stderr, "Undefined port number\n");
        }
        return -1;
    }

    /* Validate path */
    if (!options.role_path) {
        if (options.verbose) {
            fprintf(stderr, "No path provided\n");
        }
        return -1;
    }
    if (strlen(file_path) == 0) {
        if (options.verbose) {
            fprintf(stderr, "Empty file path %s\n", file_path);
        }
        return -1;
    }

    /* Validate bytes per packet */
    if (role == TRANSMITTER) {
        if (options.bytes_per_packet) {
            if (bytes_per_packet <= 0 ||
                bytes_per_packet > MAX_DATA_PER_PACKET_SIZE) {
                fprintf(stderr, "Invalid bytes per packet\n");
                return -1;
            }
        } else if (options.verbose) {
            printf("No bytes per packet value given, using default size: %d "
                   "bytes\n",
                   DEFAULT_BYTES_PER_PACKET);
        }
    } else if (options.bytes_per_packet) {
        fprintf(stderr,
                "Application can't specify bytes per packet when receiving.\n");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    if (parse_options(argc, argv) == -1) {
        print_usage(argv);
        return -1;
    }

    if (assert_valid_options() == -1) {
        print_usage(argv);
        return -1;
    }

    if (options.verbose) {
        printf("port: %d, role: %d, filepath: %s, filename: %s", port, role,
               file_path, file_name);
    }

    switch (role) {
        case TRANSMITTER:
            if (send_file(file_path, file_name, port, bytes_per_packet) != 0) {
                printf("No file sent\n");
                return -1;
            }
            break;
        case RECEIVER:
            if (receive_file(file_path, file_name, port) != 0) {
                printf("No file received\n");
                return -1;
            }
            break;
    }

    return 0;
}
