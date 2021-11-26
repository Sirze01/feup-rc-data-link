#include "packet.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

int assemble_data_packet(char *out_packet, int seq_no, char *data,
                         unsigned data_size) {
    out_packet[0] = DP_CONTROL;
    out_packet[1] = DP_SEQ_NO(seq_no);
    out_packet[2] = data_size / 256;
    out_packet[3] = data_size % 256;
    memcpy((void *)(out_packet + 4), data, data_size);

    return 4 + data_size;
}

int assemble_control_packet(char *out_packet, int end, int no_args,
                            int arg1_type, int arg1_size, char *arg1_value,
                            ...) {
    out_packet[0] = end ? CP_CONTROL_END : CP_CONTROL_START;

    int curr_byte = 1;
    if (arg1_type == CP_TYPE_FILENAME) {
        out_packet[curr_byte++] = arg1_type;
        memcpy((void *)out_packet + curr_byte, &arg1_size, 2);
        curr_byte += 2;
        memcpy((void *)out_packet + curr_byte, arg1_value, arg1_size);
        curr_byte += arg1_size;
    } else {
        out_packet[curr_byte++] = arg1_type;
        memcpy((void *)out_packet + curr_byte, &arg1_size, 1);
        curr_byte++;
        memcpy((void *)out_packet + curr_byte, arg1_value, arg1_size);
        curr_byte += arg1_size;
    }

    va_list vlist;
    va_start(vlist, arg1_value);
    for (int i = 1; i < no_args; i++) {
        int type = va_arg(vlist, int);
        int size = va_arg(vlist, int);
        char *data = va_arg(vlist, char *);
        if (type == CP_TYPE_FILENAME) {
            out_packet[curr_byte++] = type;
            memcpy((void *)out_packet + curr_byte, &size, 2);
            curr_byte += 2;
            memcpy((void *)out_packet + curr_byte, data, size);
            curr_byte += size;
        } else {
            out_packet[curr_byte++] = type;
            memcpy((void *)out_packet + curr_byte, &size, 1);
            curr_byte++;
            memcpy((void *)out_packet + curr_byte, data, size);
            curr_byte += size;
        }
    }
    va_end(vlist);

    return curr_byte;
}
