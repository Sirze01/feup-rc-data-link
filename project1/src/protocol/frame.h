#pragma once

/* COMMON TO BOTH FRAMES */
#define F_FLAG 0x07e
#define F_ESCAPE_CHAR 0x7d

/* SUPERVISION/NOT NUMBERED FRAME */
/* F A C BCC1 F */
#define SUF_FRAME_SIZE 5 * sizeof(char)
#define SUF_CONTROL_SET 0x03
#define SUF_CONTROL_DISC 0x0b
#define SUF_CONTROL_UA 0x07
#define SUF_CONTROL_RR(pckt) 0x05 | (pckt << 7)
#define SUF_CONTROL_REJ(pckt) 0x01 | (pckt << 7)

/* INFORMATION FRAME */
/* F A C BCC1 Data BCC2 F */
#define IF_FIELDS_SIZE 6 * sizeof(char)
#define IF_ADDRESS_SENDER_COMMANDS 0x03
#define IF_ADDRESS_RECEIVER_COMMANDS 0x01
#define IF_CONTROL(no_seq) (no_seq << 6)

typedef enum DeviceRole { TRANSMITTER, RECEIVER } DeviceRole;

typedef struct SUFrame {
    char flg;
    char addr;
    char ctr;
    char bcc1;
    char *frame;
} SUFrame;

typedef struct IFrame {
    char flg;
    char addr;
    char ctr;
    char bcc1;
    char bcc2;
    int data_size;
    char *data;
    char *frame;
} IFrame;

char byte_xor(char *data, int size);
int stuff_bytes(char *data, int size);
int destuff_bytes(char *data, int size);
SUFrame *assemble_suframe(DeviceRole role, char ctr);
IFrame *assemble_iframe(DeviceRole role, char ctr, int size, char *data);

void free_SUFrame(SUFrame *frame);

void free_IFrame(IFrame *frame);
