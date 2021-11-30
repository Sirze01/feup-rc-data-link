#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../protocol/data_link.h"
#include "packet.h"
#include "sender.h"
#include "utils.h"

static unsigned char packet[MAX_PACKET_SIZE];

void assemble_packet_file_name(char *out_packet_file_name, char *file_name,
                               char *file_path) {
    if (strlen(file_name) == 0) {
        char *last_name = strrchr(file_path, '/');
        if (last_name != NULL) {
            strncpy(out_packet_file_name, last_name + 1, PATH_MAX);
        } else {
            strncpy(out_packet_file_name, file_path, PATH_MAX);
        }
    } else {
        strncpy(out_packet_file_name, file_name, PATH_MAX);
    }
}

int send_control_packet(int port_fd, unsigned file_size, int bytes_per_packet,
                        char *packet_file_name, int is_end) {
    int packet_size = assemble_control_packet(
        packet, is_end, file_size, bytes_per_packet, packet_file_name);
    if (llwrite(port_fd, packet, packet_size) < packet_size) {
        return -1;
    }
    return 0;
}

int send_file_data(int port_fd, int fd, int bytes_per_packet, int file_size) {
    unsigned char data[MAX_DATA_PER_PACKET_SIZE];
    unsigned char seq_no = 0;
    unsigned curr_byte = 0;
    for (;;) {
        int read_bytes = read(fd, data, bytes_per_packet);
        if (read_bytes < 0) {
            fprintf(stderr,
                    "Failed reading data from file for data packet %u\n",
                    seq_no);
            return -1;
        } else if (read_bytes == 0) {
            break;
        } else {
            int packet_size =
                assemble_data_packet(packet, seq_no, data, read_bytes);
            if (llwrite(port_fd, packet, packet_size) < packet_size) {
                fprintf(stderr, "Failed writing data packet %d\n", seq_no);
                return -1;
            }
            seq_no++;
            curr_byte += read_bytes;
            print_progress_bar(curr_byte, file_size);
        }
    }
    return 0;
}