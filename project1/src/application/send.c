#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../protocol/data_link.h"
#include "packet.h"
#include "send.h"

static char packet_file_name[PATH_MAX];

static void print_bytes(char *buf, int size) {
    printf("size: %d\n", size);
    for (int i = 0; i < size; i++) {
        printf("%x ", buf[i]);
    }
    printf("\n\n");
}

int send_file(char *file_path, char *file_name, int port,
              int bytes_per_packet) {
    if (strlen(file_name) < 2) {
        char *base_name = strrchr(file_path, '/');
        if (base_name != NULL) {
            strncpy(packet_file_name, base_name, strlen(base_name) + 1);
        } else {
            strncpy(packet_file_name, file_path, strlen(file_path) + 1);
        }
    } else {
        strncpy(packet_file_name, file_name, strlen(file_name) + 1);
    }
    int packet_file_name_len = strlen(packet_file_name);

    /* Retrieve file info */
    struct stat st;
    if (stat(file_path, &st) == -1) {
        perror("Retrieve file info");
        return -1;
    }

    /* Open file for reading */
    int fd = -1;
    fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        perror("Open file");
        return -1;
    }

    /* Open stream */
    int port_fd = -1;
    if ((port_fd = llopen(port, TRANSMITTER)) == -1) {
        fprintf(stderr, "Can't open port\n");
        if (close(fd) == -1) {
            perror("Closing File");
        }
        return -1;
    }

    /* Send start packet */
    char packet[MAX_PACKET_SIZE];
    int packet_size = -1;
    packet_size =
        assemble_control_packet(packet, 0, st.st_size, bytes_per_packet,
                                packet_file_name, packet_file_name_len);
    if (llwrite(port_fd, packet, packet_size) < packet_size) {
        fprintf(stderr, "Failed writing start control packet\n");
        if (llclose(port_fd) == -1) {
            fprintf(stderr, "Failed closing port\n");
        }
        if (close(fd) == -1) {
            perror("Closing file");
        }
        return -1;
    }

    /* Send file to stream at given rate */
    char data[MAX_DATA_PER_PACKET_SIZE];
    unsigned seq_no = 0;
    int bytes_read = -1;
    for (int i = 0; i < st.st_size; i += bytes_read) {
        if ((bytes_read = read(fd, data, bytes_per_packet)) !=
            bytes_per_packet) {
            if (bytes_read < 0) {
                perror("Reading from file");
                fprintf(stderr,
                        "Failed reading data from file for data packet %d\n",
                        seq_no++);
                break;
            }
        }

        packet_size = assemble_data_packet(packet, seq_no++, data, bytes_read);
        if (llwrite(port_fd, packet, packet_size) < packet_size) {
            fprintf(stderr, "Failed writing data packet %d\n", seq_no);
            break;
        }
    }

    /* Send end packet */
    packet_size =
        assemble_control_packet(packet, 1, st.st_size, bytes_per_packet,
                                packet_file_name, packet_file_name_len);

    if (llwrite(port_fd, packet, packet_size) < packet_size) {
        printf("Failed writing end control packet\n");
        if (llclose(port_fd) == -1) {
            fprintf(stderr, "Failed closing port\n");
        }
        if (close(fd) == -1) {
            perror("Close file");
        }
        return -1;
    }

    /* Close stream */
    if (llclose(port_fd) == -1) {
        printf("Failed closing port\n");
        if (close(fd) == -1) {
            perror("Closing file");
        }
        return -1;
    }

    /* Close file */
    if (close(fd) == -1) {
        perror("Closing file");
        return -1;
    }

    return 0;
}
