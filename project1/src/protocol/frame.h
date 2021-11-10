/* INFORMATION FRAME */
/* F A C BCC1 Data BCC2 F */
#define IF_ADDRESS_SENDER_COMMANDS 0x03
#define IF_ADDRESS_RECEIVER_COMMANDS 0x01
#define IF_CONTROL(no_seq) (no_seq << 6)

/* SUPERVISION/NOT NUMBERED FRAME */
/* F A C BCC1 F */
#define SUF_CONTROL_SET 0x03
#define SUF_CONTROL_DISC 0x0b
#define SUF_CONTROL_UA 0x07
#define SUF_CONTROL_RR(pckt) 0x05 | (pckt << 7)
#define SUF_CONTROL_REJ(pckt) 0x01 | (pckt << 7)

/* COMMON TO BOTH FRAMES */
#define F_FLAG 0x07e
#define F_ESCAPE_CHAR 0x7d

char BCC1(char A, char C);
char BBC2(char *data, int size);
int byte_stuff(char *data, int size);
int byte_destuff(char *data, int size);