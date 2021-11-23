#pragma once

#include <linux/limits.h>
#include <stdbool.h>

/* COMMON TO BOTH FRAMES */
#define F_FLAG 0x7e
#define F_ESCAPE_CHAR 0x7d
#define F_ADDRESS_TRANSMITTER_COMMANDS 0x03
#define F_ADDRESS_RECEIVER_COMMANDS 0x01

/* SUPERVISION/NOT NUMBERED FRAME */
/* F A C BCC1 F */
#define SUF_FRAME_SIZE 5
#define SUF_CONTROL_SET 0x03
#define SUF_CONTROL_DISC 0x0b
#define SUF_CONTROL_UA 0x07
#define SUF_CONTROL_RR(pckt) 0x05 | (pckt << 7)
#define SUF_CONTROL_REJ(pckt) 0x01 | (pckt << 7)

/* INFORMATION FRAME */
/* F A C BCC1 Data BCC2 F */
#define IF_FRAME_SIZE 8192
#define IF_MAX_DATA_SIZE 4093
#define IF_CONTROL(no_seq) (no_seq << 6)

typedef enum device_role { TRANSMITTER, RECEIVER } device_role;

char byte_xor(char *data, int size);

int stuff_bytes(char *data, int data_size);
int destuff_bytes(char *data, int data_size);

void assemble_suframe(char *out_frame, device_role role, char ctr);
int assemble_iframe(char *out_frame, device_role role, char ctr,
                    char *unstuffed_data, int unstuffed_data_size);
