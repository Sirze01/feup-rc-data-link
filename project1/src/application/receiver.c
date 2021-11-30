#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../protocol/data_link.h"
#include "packet.h"
#include "receiver.h"

static char packet[MAX_PACKET_SIZE];
static int file_size = -1;
static char file_name[PATH_MAX] = "";
static int bytes_per_packet = -1;

int read_validate_start_packet(int port_fd, char *out_file_name) {
    /* Read start packet */
    int packet_length = -1;
    if ((packet_length = llread(port_fd, packet)) == -1) {
        fprintf(stderr, "Can't read start packet\n");
        return -1;
    }

    /* Validate start packet */
    if (packet[0] != CP_CONTROL_START) {
        return -2;
    }
    int file_name_length = -1;
    for (int i = 1; i < packet_length;) {
        switch (packet[i]) {
            case CP_TYPE_SIZE:
                memcpy(&file_size, packet + i + 2, packet[i + 1]);
                i += (2 + packet[i + 1]);
                break;
            case CP_TYPE_FILENAME:
                memcpy(&file_name_length, packet + i + 1, sizeof(int));
                i += (1 + sizeof(int));
                memcpy(file_name, packet + i, file_name_length);
                strncpy(out_file_name, file_name, file_name_length);
                i += file_name_length;
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
    return 0;
}

int write_file_from_stream(int port_fd, int fd) {
    int bytes_read = -1;
    int curr_file_size = 0;
    for (int seq_no = 0;; seq_no = (seq_no + 1) % MAX_SEQ_NO) {
        if ((bytes_read = llread(port_fd, packet)) < 0) {
            return -1;
        }
        if (packet[0] != DP_CONTROL) {
            return -1;
        }
        if (packet[1] != DP_SEQ_NO(seq_no)) {
            return -1;
        }
        int no_bytes = packet[2] * 256 + packet[3];
        if (write(fd, &packet[4], no_bytes) == -1) {
            perror("Write file");
            return -1;
        }
        curr_file_size += no_bytes;
        if (curr_file_size >= file_size) {
            break;
        }
    }

    return 0;
}

int read_validate_end_packet(int port_fd) {
    /* Read end packet */
    int packet_length = -1;
    if ((packet_length = llread(port_fd, packet)) == -1) {
        fprintf(stderr, "Can't read end packet\n");
        return -1;
    }

    /* Validate end packet */
    if (packet[0] != CP_CONTROL_END) {
        return -2;
    }
    int nfile_name_length = -1, nfile_size = -1, nbytes_per_packet = -1;
    char nfile_name[PATH_MAX];
    for (int i = 1; i < packet_length;) {
        switch (packet[i]) {
            case CP_TYPE_SIZE:
                memcpy(&nfile_size, packet + i + 2, packet[i + 1]);
                i += (2 + packet[i + 1]);
                if (nfile_size != file_size) {
                    return -2;
                }
                break;
            case CP_TYPE_FILENAME:
                memcpy(&nfile_name_length, packet + i + 1, sizeof(int));
                i += (1 + sizeof(int));
                memcpy(&nfile_name, packet + i, nfile_name_length);
                if (strcmp(nfile_name, file_name) != 0) {
                    return -2;
                }
                i += nfile_name_length;
                break;
            case CP_TYPE_BYTES_PER_PACKET:
                memcpy(&nbytes_per_packet, packet + i + 2, packet[i + 1]);
                i += (2 + packet[i + 1]);
                if (nbytes_per_packet != bytes_per_packet) {
                    return -2;
                }
                break;
            default:
                fprintf(stderr, "Unsupported parameter type\n");
                return -2;
        }
    }
    return 0;
}