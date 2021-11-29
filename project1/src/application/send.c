#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../protocol/data_link.h"
#include "packet.h"
#include "send.h"

static char packet[MAX_PACKET_SIZE];
static char file_name[PATH_MAX];
static char file_path[PATH_MAX];
static int file_size = -1;
static int fd = -1;
static int port_fd = -1;

/*static void print_bytes(char *buf, int size) {
    printf("size: %d\n", size);
    for (int i = 0; i < size; i++) {
        printf("%x ", buf[i]);
    }
    printf("\n\n");
}*/

static void assemble_packet_file_name(char *packet_file_name) {
    if (strlen(file_name) == 0) {
        char *last_name = strrchr(file_path, '/');
        if (last_name != NULL) {
            strncpy(packet_file_name, last_name + 1, strlen(last_name + 1) + 1);
        } else {
            strncpy(packet_file_name, file_path, strlen(file_path) + 1);
        }
    } else {
        strncpy(packet_file_name, file_name, strlen(file_name) + 1);
    }
}

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

static int send_control_packet(int bytes_per_packet, char *packet_file_name,
                               int is_end) {
    int packet_size = assemble_control_packet(
        packet, is_end, file_size, bytes_per_packet, packet_file_name);
    if (llwrite(port_fd, packet, packet_size) < packet_size) {
        return -1;
    }
    return 0;
}

static int send_file_data(int bytes_per_packet) {
    char data[MAX_DATA_PER_PACKET_SIZE];
    int bytes_read = -1, seq_no = 0, packet_size = 0;
    for (int i = 0; i < file_size; i += bytes_read) {
        if ((bytes_read = read(fd, data, bytes_per_packet)) < 0) {
            fprintf(stderr,
                    "Failed reading data from file for data packet %d\n",
                    seq_no);
            return -1;
        }

        packet_size = assemble_data_packet(packet, seq_no, data, bytes_read);
        if (llwrite(port_fd, packet, packet_size) < packet_size) {
            fprintf(stderr, "Failed writing data packet %d\n", seq_no);
            break;
        }
        seq_no = (seq_no + 1) % 10000;
    }
    return 0;
}

int send_file(char *fpath, char *fname, int port, int bytes_per_packet) {
    /* Copy to local buffers */
    strncpy(file_path, fpath, PATH_MAX);
    strncpy(file_name, fname, PATH_MAX);

    /* Retrieve file info */
    struct stat st;
    if (stat(file_path, &st) == -1) {
        perror("Retrieve file info");
        return -1;
    }
    file_size = st.st_size;

    /* Open file for reading */
    fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Open file");
        return -1;
    }
    atexit(close_fd);

    /* Open stream */
    if ((port_fd = llopen(port, TRANSMITTER)) == -1) {
        fprintf(stderr, "Can't open port\n");
        return -1;
    }
    atexit(close_stream);

    /* Send start packet */
    char packet_file_name[MAX_FILENAME_LENGTH];
    assemble_packet_file_name(packet_file_name);
    if (send_control_packet(bytes_per_packet, packet_file_name, 0) == -1) {
        fprintf(stderr, "Failed writing control packet\n");
        return -1;
    }

    /* Send file to stream at given rate */
    // integrate with progress bar?
    if (send_file_data(bytes_per_packet) == -1) {
        return -1;
    }

    /* Send end packet */
    if (send_control_packet(bytes_per_packet, packet_file_name, 1) == -1) {
        return -1;
    }

    return 0;
}
