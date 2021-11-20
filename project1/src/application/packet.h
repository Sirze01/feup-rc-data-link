#pragma once

/* CONTROL PACKET */
#define CP_CONTROL_START 2
#define CP_CONTROL_END 2

/* DATA PACKET */
#define DP_CONTROL 1
#define DP_SEQ_NO(x) (x % 256)

void assemble_data_packet(int seq_no, unsigned char* out_packet, unsigned char* data, unsigned data_size);
void assemble_control_packet(int end, int file_size, char* file_name);