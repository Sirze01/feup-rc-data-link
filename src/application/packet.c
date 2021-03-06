#include <stdio.h>
#include <string.h>

#include "packet.h"

int assemble_data_packet(unsigned char *out_packet, unsigned char seq_no,
                         unsigned char *data, unsigned data_size) {
    out_packet[0] = DP_CONTROL;
    out_packet[1] = DP_SEQ_NO(seq_no);
    out_packet[2] = data_size / 256;
    out_packet[3] = data_size % 256;
    memcpy((void *)(out_packet + 4), data, data_size);

    return 4 + data_size;
}

int assemble_control_packet(unsigned char *out_packet, int end,
                            unsigned file_size, unsigned bytes_per_packet,
                            char *file_name) {
    out_packet[0] = end ? CP_CONTROL_END : CP_CONTROL_START;

    unsigned curr_byte = 1;
    /* File size */
    out_packet[curr_byte++] = CP_TYPE_SIZE;
    out_packet[curr_byte++] = sizeof(unsigned);
    memcpy(out_packet + curr_byte, &file_size, sizeof(unsigned));
    curr_byte += sizeof(unsigned);

    /* File name */
    unsigned file_name_size = strlen(file_name) + 1;
    out_packet[curr_byte++] = CP_TYPE_FILENAME;
    memcpy(out_packet + curr_byte, &file_name_size, sizeof(unsigned));
    curr_byte += sizeof(unsigned);
    curr_byte += snprintf((char *)out_packet + curr_byte, file_name_size, "%s",
                          file_name) +
                 1;

    /* Max bytes sent per packet */
    out_packet[curr_byte++] = CP_TYPE_BYTES_PER_PACKET;
    out_packet[curr_byte++] = sizeof(unsigned);
    memcpy(out_packet + curr_byte, &bytes_per_packet, sizeof(unsigned));
    curr_byte += sizeof(unsigned);

    return curr_byte;
}
