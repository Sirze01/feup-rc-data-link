#pragma once

#define MAX_PACKET_SIZE 4096
#define MAX_DATA_PER_PACKET_SIZE MAX_PACKET_SIZE - 4

/* CONTROL PACKET */
#define CP_CONTROL_START 2
#define CP_CONTROL_END 3
#define CP_TYPE_SIZE 0
#define CP_TYPE_FILENAME 1

/* DATA PACKET */
#define DP_CONTROL 1
#define DP_SEQ_NO(x) (x % 256)

int assemble_data_packet(char *out_packet, int seq_no, char *data,
                         unsigned data_size);
int assemble_control_packet(char *out_packet, int end, int no_args,
                            int arg1_type, int arg1_size, char *arg1_value,
                            ...);