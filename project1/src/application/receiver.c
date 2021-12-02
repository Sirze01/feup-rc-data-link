#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../protocol/data_link.h"
#include "packet.h"
#include "receiver.h"
#include "utils.h"

static unsigned char ctrl_packet[MAX_PACKET_SIZE];
static unsigned char packet[MAX_PACKET_SIZE];
static unsigned file_size = -1;
static char file_name[PATH_MAX] = "";
static unsigned bytes_per_packet = -1;

unsigned get_receiver_bytes_per_packet() {
    return bytes_per_packet;
}

int read_validate_control_packet(int port_fd, char *out_file_name, int is_end) {
    /* Read start packet */
    int packet_length = -1;
    if ((packet_length = llread(port_fd, packet)) == -1) {
        return -1;
    }
    if (!is_end) {
        memcpy(ctrl_packet, packet, packet_length);
    }

    /* Validate arguments */
    unsigned char c = is_end ? CP_CONTROL_END : CP_CONTROL_START;
    if (packet[0] != c) {
        return -2;
    }
    unsigned file_name_size = 0;
    for (int i = 1; i < packet_length;) {
        switch (packet[i]) {
            case CP_TYPE_SIZE:
                memcpy(&file_size, packet + i + 2, packet[i + 1]);
                i += (2 + packet[i + 1]);
                break;
            case CP_TYPE_FILENAME:
                memcpy(&file_name_size, packet + i + 1, sizeof(unsigned));
                i += (1 + sizeof(unsigned));
                i += snprintf(file_name, file_name_size, "%s",
                              (char *)packet + i) +
                     1;
                strncpy(out_file_name, file_name, file_name_size - 1);
                break;
            case CP_TYPE_BYTES_PER_PACKET:
                memcpy(&bytes_per_packet, packet + i + 2, packet[i + 1]);
                i += (2 + packet[i + 1]);
                break;
            default:
                fprintf(stderr, "Unsupported parameter type\n");
                return -1;
        }
    }

    /* Compare with start packet if end packet */
    if (is_end && memcmp(packet + 1, ctrl_packet + 1, packet_length - 1) != 0) {
        return -1;
    }
    return 0;
}

int write_file_from_stream(int port_fd, int fd) {
    unsigned curr_file_size = 0;
    unsigned char seq_no = 0;
    for (;;) {
        if (llread(port_fd, packet) < 0) {
            fprintf(stderr, "\nFailed reading file at offset %u\n",
                    curr_file_size);
            return -1;
        }
        if (packet[0] != DP_CONTROL) {
            fprintf(stderr, "\nByte control for file at offset %u is wrong\n",
                    curr_file_size);
            return -1;
        }
        if (packet[1] != DP_SEQ_NO(seq_no)) {
            fprintf(stderr,
                    "\nSequence number for file at offset %u is wrong\n",
                    curr_file_size);
            return -1;
        }
        int no_bytes = packet[2] * 256 + packet[3];
        if (write(fd, &packet[4], no_bytes) == -1) {
            perror("\nWrite file");
            return -1;
        }
        curr_file_size += no_bytes;
        print_transfer_progress_bar(curr_file_size, file_size);
        if (curr_file_size >= file_size) {
            break;
        }
        seq_no++;
    }
    printf("\n");
    return 0;
}
