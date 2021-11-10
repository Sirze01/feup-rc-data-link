/** INFORMATION FRAME **/
/** F A C BCC1 Data BCC2 F **/
#define IF_FLAG 0x07E
#define IF_ADDRESS_SENDER_COMMANDS 0x03
#define IF_ADDRESS_RECEIVER_COMMANDS 0x01
#define IF_CONTROL(no_seq) (no_seq << 6)
#define IF_BCC1(A, C) A ^ C
#define IF_BCC2(D1, ...) D1 ^ __VA_ARGS__

#define SUF_FLAG 0x07E
#define SUF_CONTROL_SET 0x03
#define SUF_CONTROL_DISC 0x0B
#define SUF_CONTROL_UA 0x07
#define SUF_CONTROL_RR(pckt) 0x05 | (pckt << 7)
#define SUF_CONTROL_REJ(pckt) 0x01 | (pckt << 7)
