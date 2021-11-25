#pragma once

/* CONTROL PACKET */
#define CP_CONTROL_START 2
#define CP_CONTROL_END 2

/* DATA PACKET */
#define DP_CONTROL 1
#define DP_SEQ_NO(x) (x % 256)

void assemble_data_packet(char *out_packet, int seq_no, char *data,
                          unsigned data_size);
void assemble_control_packet(char *out_packet, int end, int no_args,
                             int arg1_size, char *arg1, ...);