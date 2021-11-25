#include "packet.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void assemble_data_packet(char *out_packet, int seq_no, char *data,
                          unsigned data_size) {
    out_packet[0] = DP_CONTROL;
    out_packet[1] = DP_SEQ_NO(seq_no);
    out_packet[2] = data_size / 256;
    out_packet[3] = data_size % 256;
    memcpy((void *)(out_packet + 4), data, data_size);
}

void assemble_control_packet(char *out_packet, int end, int no_args,
                             int arg1_size, char *arg1, ...) {
    out_packet[0] = end ? CP_CONTROL_END : CP_CONTROL_START;

    int curr_byte = 1;
    out_packet[curr_byte++] = arg1_size;
    memcpy((void *)out_packet + curr_byte, arg1, arg1_size);
    curr_byte += arg1_size;

    va_list vlist;
    va_start(vlist, arg1);
    for (int i = 1; i < no_args; i++) {
        int size = va_arg(vlist, int);
        char *data = va_arg(vlist, char *);
        out_packet[curr_byte++] = size;
        memcpy((void *)out_packet + curr_byte, data, size);
        curr_byte += size;
    }
    va_end(vlist);
}
