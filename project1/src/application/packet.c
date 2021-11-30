#include <stdio.h>
#include <string.h>

#include "packet.h"

int assemble_data_packet(char *out_packet, int seq_no, char *data,
                         unsigned data_size) {
    out_packet[0] = DP_CONTROL;
    out_packet[1] = DP_SEQ_NO(seq_no);
    out_packet[2] = data_size / 256;
    out_packet[3] = data_size % 256;
    memcpy((void *)(out_packet + 4), data, data_size);

    return 4 + data_size;
}

int assemble_control_packet(char *out_packet, int end, int file_size,
                            int bytes_per_packet, char *file_name) {
    out_packet[0] = end ? CP_CONTROL_END : CP_CONTROL_START;

    int curr_byte = 1;
    /* File size */
    out_packet[curr_byte++] = CP_TYPE_SIZE;
    out_packet[curr_byte++] = sizeof(int);
    memcpy(out_packet + curr_byte, &file_size, sizeof(int));
    curr_byte += sizeof(int);

    /* File name */
    int file_name_length = strlen(file_name);
    out_packet[curr_byte++] = CP_TYPE_FILENAME;
    memcpy(out_packet + curr_byte, &file_name_length, sizeof(int));
    curr_byte += sizeof(int);
    strncpy(out_packet + curr_byte, file_name, file_name_length);
    curr_byte += file_name_length;

    /* Max bytes sent per packet */
    out_packet[curr_byte++] = CP_TYPE_BYTES_PER_PACKET;
    out_packet[curr_byte++] = sizeof(int);
    memcpy(out_packet + curr_byte, &bytes_per_packet, sizeof(int));
    curr_byte += sizeof(int);

    return curr_byte;
}
