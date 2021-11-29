#pragma once

#define MAX_FILENAME_LENGTH 1024
#define MAX_FILE_PATH_LENGTH 3072

/* CONTROL PACKET */
#define CP_CONTROL_START 2
#define CP_CONTROL_END 3
#define CP_TYPE_SIZE 0
#define CP_TYPE_FILENAME 1
#define CP_TYPE_BYTES_PER_PACKET 2

/* DATA PACKET */
#define DP_CONTROL 1
#define DP_SEQ_NO(x) (x % 256)

int assemble_data_packet(char *out_packet, int seq_no, char *data,
                         unsigned data_size);
int assemble_control_packet(char *out_packet, int end, int file_size,
                            int bytes_per_packet, char *file_name,
                            int file_name_length);