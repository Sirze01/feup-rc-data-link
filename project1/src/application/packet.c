#include "packet.h"
#include <string.h>

void assemble_data_packet(int seq_no, unsigned char *out_packet,
                          unsigned char *data, unsigned data_size) {
    out_packet[0] = DP_CONTROL;
    out_packet[1] = DP_SEQ_NO(seq_no);
    out_packet[2] = data_size > 255 ? data_size - data_size % 256 - 255 : 0;
    out_packet[3] = data_size % 256;
    memcpy((void *)(out_packet + 4), data, data_size);
}

void assemble_control_packet(int end, int file_size, char *file_name) {
}