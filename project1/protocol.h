#define FLAG 0x07E

#define ADDRESS_SENDER_COMMANDS 0x03 
#define ADDRESS_RECEIVER_COMMANDS 0x01

#define CONTROL_SET 0x03
#define CONTROL_DISC 0x0B
#define CONTROL_UA 0x07
#define CONTROL_RR 0x05  // Needs bitmask with packet number
#define CONTROL_REJ 0x01 // Needs bitmask with packet number
