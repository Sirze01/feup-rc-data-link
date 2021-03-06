#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../data_link/data_link.h"
#include "receiver.h"
#include "sender.h"
#include "utils.h"

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
static char file_name[PATH_MAX / 4];
static char file_path[PATH_MAX / 4];
static int fd = -1;
static int port_fd = -1;

#define verbose_printf                                                         \
    if (options.verbose)                                                       \
    printf

#define verbose_fprintf                                                        \
    if (options.verbose)                                                       \
    fprintf

/**
 * @brief Closes the file  (Registered with atexit())
 *
 */
static void close_fd() {
    if (close(fd) == -1) {
        perror("Close file");
    }
}

/**
 * @brief Closes the port file (Registered with atexit())
 *
 */
static void close_stream() {
    verbose_printf("Trying to close stream...\n");
    if (llclose(port_fd) == -1) {
        fprintf(stderr, "Failed closing stream\n");
        return;
    }
    verbose_printf("Stream closed\n");
}

/**
 * @brief Sends file, getting info from the file, opening it, opening port,
 * sending the start packet, reading the file and sending it, sending the end
 * control packet and closing the file and port fd's.
 *
 * @param port Port number
 * @return int -1 in case of error, 0 otherwise
 */
int send_file(int port) {
    /* Get file size */
    struct stat st;
    if (stat(file_path, &st) == -1) {
        perror("Get file info");
        return -1;
    }

    if (st.st_size > (off_t)UINT_MAX) {
        fprintf(stderr, "File size is too large\n");
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
    verbose_printf("Trying to open stream on /dev/ttyS%d...\n", port);
    if ((port_fd = llopen(port, TRANSMITTER)) == -1) {
        fprintf(stderr, "Failed opening connection\n");
        return -1;
    }
    verbose_printf("Stream open\n");
    atexit(close_stream);

    /* Send start packet */
    verbose_printf("Sending start packet...\n");
    char packet_file_name[PATH_MAX];
    assemble_packet_file_name(packet_file_name, file_name, file_path);
    if (send_control_packet(port_fd, st.st_size, bytes_per_packet,
                            packet_file_name, 0) == -1) {
        fprintf(stderr, "Failed writing start packet\n");
        return -1;
    }
    verbose_printf("Sent start packet\n");

    /* Send file to stream */
    if (send_file_data(port_fd, fd, bytes_per_packet, st.st_size) == -1) {
        return -1;
    }

    /* Send end packet */
    verbose_printf("Sending end packet...\n");
    if (send_control_packet(port_fd, st.st_size, bytes_per_packet,
                            packet_file_name, 1) == -1) {
        fprintf(stderr, "Failed writing end packet\n");
        return -1;
    }
    verbose_printf("Sent end packet\n");

    return 0;
}

/**
 * @brief Receives a file, opening the port, receiving the start control packet,
 * creating a file with name according to the start packet name field, reading
 * from stream and writing to the file the content of the data field, receiving
 * the end packet and closing the file streams, timing and collecting
 * information (FER, Elapsed time and Average speed) on the file data retrieval
 * process.
 *
 * @param port Port number
 * @return int -1 in case of error, 0 otherwise
 */
int receive_file(int port) {
    /* Open stream */
    verbose_printf("Trying to open stream on /dev/ttyS%d...\n", port);
    if ((port_fd = llopen(port, RECEIVER)) == -1) {
        fprintf(stderr, "Failed opening connection\n");
        return -1;
    }
    verbose_printf("Stream open\n");
    atexit(close_stream);

    /* Read start packet */
    verbose_printf("Reading start packet...\n");
    if (read_validate_control_packet(port_fd, file_name, 0) != 0) {
        fprintf(stderr, "Start packet is not valid\n");
        return -1;
    }
    verbose_printf("Read start packet\n");

    /* Make new file name if file exists */
    char file_path_[PATH_MAX];
    snprintf(file_path_, PATH_MAX, "%s/%s", file_path, file_name);
    for (int n = 1;; n++) {
        if (access(file_path_, F_OK) == 0) {
            snprintf(file_path_, PATH_MAX, "%s/%d-%s", file_path, n, file_name);
        } else {
            if (n > 1) {
                strncpy(file_name, strrchr(file_path_, '/') + 1, PATH_MAX / 4);
                verbose_printf(
                    "File name already exists, creating new file: %s\n",
                    file_name);
            }
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
    struct timespec start_time, end_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1) {
        perror("Clock get start time");
    }
    if (write_file_from_stream(port_fd, fd) != 0) {
        return -1;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &end_time) == -1) {
        perror("Clock get end time");
    }

    /* Validate end packet */
    verbose_printf("Reading end packet...\n");
    if (read_validate_control_packet(port_fd, NULL, 1) != 0) {
        fprintf(stderr, "End packet is not valid\n");
        return -1;
    }
    verbose_printf("Read end packet\n");

    /* Print statistics */
    if (options.verbose) {
        struct stat st;
        if (stat(file_path_, &st) == -1) {
            perror("Stat");
        } else {
            int errors = llgeterrors();
            int no_packets = st.st_size / get_receiver_bytes_per_packet();
            float percentage = (float)errors / (float)no_packets;
            int percentage_ = (int)(percentage * 100);
            printf("Error rate: %i%% (%d errors in %d packets)\n", percentage_,
                   errors, no_packets);
            double elapsed_secs = elapsed_seconds(&start_time, &end_time);
            double kbs = ((double)st.st_size / 1000) / elapsed_secs;
            printf("Elapsed time: %.2fs\n", elapsed_secs);
            printf("Average speed: %.2fKB/s\n", kbs);
        }
    }

    return 0;
}

/**
 * @brief Prints application usage help message.
 *
 * @param argv Argv pointer to get the invocation method
 */
static void print_usage(char **argv) {
    printf("Usage: %s [-v] -p <port> -s <filepath> -r <outdirectory> [-n "
           "filename] [-b <dataperpacket>]\n",
           argv[0]);
}

/**
 * @brief Function to parse the command options.
 *
 * @param argc Argument counter
 * @param argv Argument vector
 * @return int -1 in case of error, 0 otherwise
 */
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
                if (strlen(optarg) >= PATH_MAX / 4) {
                    printf("Path length is too large\n");
                    return -1;
                }
                strncpy(file_path, optarg, PATH_MAX / 4);
                role = opt == 's' ? TRANSMITTER : RECEIVER;
                options.role_path = true;
                break;
            case 'n':
                if (strlen(optarg) >= PATH_MAX / 4) {
                    printf("Name length is too large\n");
                    return -1;
                }
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

/**
 * @brief Validates the options parsed from parse_options()
 *
 * @return int -1 in case of error, 0 otherwise
 */
int assert_valid_options() {
    /* Print immediately */
    setbuf(stdout, NULL);

    /* Validate port */
    if (!options.port) {
        verbose_fprintf(stderr, "Undefined port number\n");
        return -1;
    }

    /* Validate path */
    if (!options.role_path) {
        verbose_fprintf(stderr, "No path provided\n");
        return -1;
    }
    if (strlen(file_path) == 0) {
        verbose_fprintf(stderr, "Empty file path");
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
        } else {
            verbose_printf(
                "No bytes per packet value given, using default size: %d "
                "bytes\n",
                DEFAULT_BYTES_PER_PACKET);
        }
    } else {
        if (options.bytes_per_packet) {
            fprintf(
                stderr,
                "Application can't specify bytes per packet when receiving\n");
            return -1;
        }
        if (options.name) {
            fprintf(stderr,
                    "Application can't specify file name when receiving\n");
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

    switch (role) {
        case TRANSMITTER:
            if (send_file(port) != 0) {
                return -1;
            }
            printf("Sent file: %s\n", file_path);
            break;
        case RECEIVER:
            if (receive_file(port) != 0) {
                return -1;
            }
            printf("Received file: %s\n", file_name);
            break;
    }
    return 0;
}
