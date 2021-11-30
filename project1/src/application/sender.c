#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../protocol/data_link.h"
#include "packet.h"
#include "sender.h"

static char packet[MAX_PACKET_SIZE];
static int last_percentage = -1;

static void print_progress_bar(int curr_byte, int file_size) {
    float percentage = (float)curr_byte / (float)file_size;
    int percentage_ = (int)(percentage * 100);
    if (percentage_ != last_percentage) {
        system("clear");
        printf("percentage: %d\n", percentage_);
        fflush(stdout);
        last_percentage = percentage_;
    }
}

void assemble_packet_file_name(char *out_packet_file_name, char *file_name,
                               char *file_path) {
    if (strlen(file_name) == 0) {
        char *last_name = strrchr(file_path, '/');
        if (last_name != NULL) {
            strncpy(out_packet_file_name, last_name + 1,
                    strlen(last_name + 1) + 1);
        } else {
            strncpy(out_packet_file_name, file_path, strlen(file_path) + 1);
        }
    } else {
        strncpy(out_packet_file_name, file_name, strlen(file_name) + 1);
    }
}

int send_control_packet(int port_fd, int file_size, int bytes_per_packet,
                        char *packet_file_name, int is_end) {
    int packet_size = assemble_control_packet(
        packet, is_end, file_size, bytes_per_packet, packet_file_name);
    if (llwrite(port_fd, packet, packet_size) < packet_size) {
        return -1;
    }
    return 0;
}

int send_file_data(int port_fd, int fd, int bytes_per_packet, int file_size) {
    char data[MAX_DATA_PER_PACKET_SIZE];
    int seq_no = 0, curr_byte = 0;
    for (;;) {
        int read_bytes = read(fd, data, bytes_per_packet);
        if (read_bytes < 0) {
            fprintf(stderr,
                    "Failed reading data from file for data packet %d\n",
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
            seq_no = (seq_no + 1) % MAX_SEQ_NO;
            curr_byte += read_bytes;
            print_progress_bar(curr_byte, file_size);
        }
    }
    return 0;
}