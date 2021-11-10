#pragma once

#include <linux/limits.h>

/* COMMON TO BOTH FRAMES */
#define F_FLAG 0x07e
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
#define IF_FRAME_SIZE PATH_MAX
#define IF_CONTROL(no_seq) (no_seq << 6)

typedef enum DeviceRole { TRANSMITTER, RECEIVER } DeviceRole;

typedef struct SUFrame {
    char frame[SUF_FRAME_SIZE];
} SUFrame;

typedef struct IFrame {
    int data_size;
    char frame[IF_FRAME_SIZE];
} IFrame;

char byte_xor(char *data, int size);
int stuff_bytes(char *data, int size);
int destuff_bytes(char *data, int size);

SUFrame assemble_suframe(DeviceRole role, char ctr);
IFrame assemble_iframe(DeviceRole role, char ctr, int size, char *data);
