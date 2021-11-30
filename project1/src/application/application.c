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
#include <unistd.h>

#include "../protocol/data_link.h"
#include "receiver.h"
#include "sender.h"

#define PATH_MAX 4096
#define DEFAULT_BYTES_PER_PACKET 100

static struct {
    bool port;
    bool role_path;
    bool name;
    bool bytes_per_packet;
    bool verbose;
} options = {false, false, false, false, false};

static device_role role;
static int port = -1;
static unsigned bytes_per_packet = DEFAULT_BYTES_PER_PACKET;
static char file_name[PATH_MAX / 4] = {};
static char file_path[PATH_MAX / 4] = {};
static int fd = -1;
static int port_fd = -1;

static void close_fd() {
    if (close(fd) == -1) {
        perror("Close file");
    }
}

static void close_stream() {
    if (llclose(port_fd) == -1) {
        fprintf(stderr, "Close port");
    }
}

int send_file(int port) {
    /* Get file file */
    struct stat st;
    if (stat(file_path, &st) == -1) {
        perror("Get file info");
        return -1;
    }

    /* Open file for reading */
    fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Open file");
        return -1;
    }
    atexit(close_fd);

    /* Open stream */
    if ((port_fd = llopen(port, TRANSMITTER)) == -1) {
        return -1;
    }
    atexit(close_stream);

    /* Send start packet */
    char packet_file_name[PATH_MAX];
    assemble_packet_file_name(packet_file_name, file_name, file_path);
    if (send_control_packet(port_fd, st.st_size, bytes_per_packet,
                            packet_file_name, 0) == -1) {
        fprintf(stderr, "Failed writing start packet\n");
        return -1;
    }

    /* Send file to stream */
    if (send_file_data(port_fd, fd, bytes_per_packet, st.st_size) == -1) {
        return -1;
    }

    /* Send end packet */
    if (send_control_packet(port_fd, st.st_size, bytes_per_packet,
                            packet_file_name, 1) == -1) {
        fprintf(stderr, "Failed writing end packet\n");
        return -1;
    }

    return 0;
}

int receive_file(int port) {
    /* Open stream */
    if ((port_fd = llopen(port, RECEIVER)) == -1) {
        return -1;
    }
    atexit(close_stream);

    /* Read start packet */
    if (read_validate_start_packet(port_fd, file_name) != 0) {
        fprintf(stderr, "Start packet is not valid\n");
        return -1;
    }

    /* Make new file name if file exists */
    char file_path_[PATH_MAX];
    snprintf(file_path_, PATH_MAX, "%s/%s", file_path, file_name);
    for (int n = 1; n < 1000; n++) {
        if (access(file_path_, F_OK) == 0) {
            snprintf(file_path_, PATH_MAX, "%s/(%d) %s", file_path, n,
                     file_name);
        } else {
            break;
        }
    }

    /* Open new file with received file name */
    if ((fd = open(file_path_, O_WRONLY | O_CREAT | O_TRUNC,
                   S_IRWXU | S_IRWXG | S_IRWXO)) == -1) {
        perror("Open file for writing");
        return -1;
    }
    atexit(close_fd);

    /* Read from stream and write to file */
    if (write_file_from_stream(port_fd, fd) != 0) {
        return -1;
    }

    /* Validate end packet */
    if (read_validate_end_packet(port_fd) != 0) {
        return -1;
    }

    return 0;
}

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
                strncpy(file_path, optarg, PATH_MAX / 4);
                role = opt == 's' ? TRANSMITTER : RECEIVER;
                options.role_path = true;
                break;
            case 'n':
                strncpy(file_name, optarg, PATH_MAX / 4);
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
    } else {
        if (options.bytes_per_packet) {
            fprintf(
                stderr,
                "Application can't specify bytes per packet when receiving.\n");
            return -1;
        }
        if (options.name) {
            fprintf(stderr,
                    "Application can't specify file name when receiving.\n");
            return -1;
        }
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
            if (send_file(port) != 0) {
                printf("Failed to send file: %s\n", file_path);
                return -1;
            }
            printf("Sent file: %s\n", file_path);
            break;
        case RECEIVER:
            if (receive_file(port) != 0) {
                printf("Failed to receive file: %s/%s\n", file_path, file_name);
                return -1;
            }
            printf("Received file: %s\n", file_name);
            break;
    }

    return 0;
}
