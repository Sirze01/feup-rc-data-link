#pragma once

#define MAX_FILENAME_LENGTH 1024
#define MAX_FILE_PATH_LENGTH 3072
#define MAX_SEQ_NO 256

/* CONTROL PACKET */
#define CP_CONTROL_START 2
#define CP_CONTROL_END 3
#define CP_TYPE_SIZE 0
#define CP_TYPE_FILENAME 1
#define CP_TYPE_BYTES_PER_PACKET 2

/* DATA PACKET */
#define DP_CONTROL 1
#define DP_SEQ_NO(x) (x % MAX_SEQ_NO)

int assemble_data_packet(unsigned char *out_packet, unsigned char seq_no,
                         unsigned char *data, unsigned data_size);
int assemble_control_packet(unsigned char *out_packet, int end,
                            unsigned file_size, unsigned bytes_per_packet,
                            char *file_name);