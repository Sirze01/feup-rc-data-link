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
#define IF_MAX_UNSTUFFED_FRAME_SIZE 4098
#define IF_MAX_DATA_SIZE 4092
#define IF_CONTROL(no_seq) (no_seq << 6)

/**
 * @brief Exclusive or recursively for array of bytes
 *
 * @param data array
 * @param size size of array
 * @return xor
 */
unsigned char byte_xor(unsigned char *data, unsigned size);

/**
 * @brief Stuff flags and escape characters. Frame should be able to hold double
 * the size of the data, in the worst case scenario. Initial and ending flags
 * are not stuffed.
 *
 * @param frame byte array
 * @param frame_size array size
 * @return stuffed frame size, -1 in case of errors
 */
int stuff_bytes(unsigned char *frame, unsigned frame_size);

/**
 * @brief Destuff flags and espace characters from byte array.
 *
 * @param frame byte array
 * @param frame_size array size
 * @return unstuffed frame size, -1 in case of errors
 */
int destuff_bytes(unsigned char *frame, unsigned frame_size);

/**
 * @brief Build supervisioned/unnumbered frame with given parameters.
 *
 * @param out_frame output buffer
 * @param role transmitter or receiver
 * @param ctr control byte
 */
void assemble_suframe(unsigned char *out_frame, int role, unsigned char ctr);

/**
 * @brief Build information frame with given parameters. Stuffs given frame
 * inside.
 *
 * @param out_frame output buffer
 * @param role receiver or transmitter
 * @param ctr control byte
 * @param unstuffed_data data array
 * @param unstuffed_data_size data array size
 * @return stuffed frame size
 */
int assemble_iframe(unsigned char *out_frame, int role, unsigned char ctr,
                    unsigned char *unstuffed_data,
                    unsigned unstuffed_data_size);

/**
 * @brief Read frame from file descriptor. Discards all bytes until a flag is
 * found. Times out if no bytes read.
 *
 * @param out_frame output buffer
 * @param max_frame_size output buffer size
 * @param fd file to read
 * @return read frame size, -1 if timeout or error
 */
int read_frame(unsigned char *out_frame, unsigned max_frame_size, int fd);
