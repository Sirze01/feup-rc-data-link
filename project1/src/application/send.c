#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../protocol/data_link.h"
#include "packet.h"
#include "send.h"

static struct stat st;

static char packet[MAX_PACKET_SIZE];
static int packet_size = -1;

static int file_fd = -1;
static int port_fd = -1;

/*static void print_bytes(char *buf, int size) {
    printf("size: %d\n", size);
    for (int i = 0; i < size; i++) {
        printf("%x ", buf[i]);
    }
    printf("\n\n");
}*/

static int send_start_packet(int bytes_per_packet, char *packet_file_name,
                             int packet_file_name_length) {
    packet_size =
        assemble_control_packet(packet, 0, st.st_size, bytes_per_packet,
                                packet_file_name, packet_file_name_length);
    if (llwrite(port_fd, packet, packet_size) < packet_size) {
        fprintf(stderr, "Failed writing start control packet\n");
        if (llclose(port_fd) == -1) {
            fprintf(stderr, "Failed closing port\n");
        }
        if (close(file_fd) == -1) {
            perror("Closing file");
        }
        return -1;
    }
    return 0;
}

static int send_file_data(int bytes_per_packet) {
    char data[MAX_DATA_PER_PACKET_SIZE];
    int bytes_read = -1;
    for (int i = 0, seq_no = 0; i < st.st_size;
         i += bytes_read, seq_no = (seq_no + 1) % 10000) {
        if ((bytes_read = read(file_fd, data, bytes_per_packet)) < 0) {
            perror("Reading from file");
            fprintf(stderr,
                    "Failed reading data from file for data packet %d\n",
                    seq_no);
            break;
        }

        packet_size = assemble_data_packet(packet, seq_no, data, bytes_read);
        if (llwrite(port_fd, packet, packet_size) < packet_size) {
            fprintf(stderr, "Failed writing data packet %d\n", seq_no);
            break;
        }
    }
    return 0;
}

static int send_end_packet(int bytes_per_packet, char *packet_file_name,
                           int packet_file_name_length) {
    packet_size =
        assemble_control_packet(packet, 1, st.st_size, bytes_per_packet,
                                packet_file_name, packet_file_name_length);

    if (llwrite(port_fd, packet, packet_size) < packet_size) {
        fprintf(stderr, "Failed writing end control packet\n");
        if (llclose(port_fd) == -1) {
            fprintf(stderr, "Failed closing port\n");
        }
        if (close(file_fd) == -1) {
            perror("Close file");
        }
        return -1;
    }
    return 0;
}

int send_file(char *file_path, char *file_name, int port,
              int bytes_per_packet) {
    /* Parse new file name -
     * If option specified, then new name = name specified, else name =
     * file_name (basename if it finds one before the first '/', the file path
     * otherwise.
     */
    char packet_file_name[MAX_FILENAME_LENGTH];
    int packet_file_name_length = -1;
    if (strlen(file_name) == 0) {
        char *base_name = strrchr(file_path, '/');
        if (base_name != NULL) {
            strncpy(packet_file_name, base_name, strlen(base_name) + 1);
        } else {
            strncpy(packet_file_name, file_path, strlen(file_path) + 1);
        }
    } else {
        strncpy(packet_file_name, file_name, strlen(file_name) + 1);
    }
    packet_file_name_length = strlen(packet_file_name);

    /* Retrieve file info */

    if (stat(file_path, &st) == -1) {
        perror("Retrieve file info");
        return -1;
    }

    /* Open file for reading */
    file_fd = open(file_path, O_RDONLY);
    if (file_fd == -1) {
        perror("Open file");
        return -1;
    }

    /* Open stream */
    if ((port_fd = llopen(port, TRANSMITTER)) == -1) {
        fprintf(stderr, "Can't open port\n");
        if (close(file_fd) == -1) {
            perror("Closing File");
        }
        return -1;
    }

    /* Send start packet */
    if (send_start_packet(bytes_per_packet, packet_file_name,
                          packet_file_name_length) == -1) {
        return -1;
    }

    /* Send file to stream at given rate */
    // integrate with progress bar?
    if (send_file_data(bytes_per_packet) == -1) {
        return -1;
    }

    /* Send end packet */
    if (send_end_packet(bytes_per_packet, packet_file_name,
                        packet_file_name_length) == -1) {
        return -1;
    }

    /* Close stream */
    if (llclose(port_fd) == -1) {
        fprintf(stderr, "Failed closing port\n");
        if (close(file_fd) == -1) {
            perror("Closing file");
        }
        return -1;
    }

    /* Close file */
    if (close(file_fd) == -1) {
        perror("Closing file");
        return -1;
    }

    return 0;
}
