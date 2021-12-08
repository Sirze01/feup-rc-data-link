#pragma once

#include "frame.h"

/**
 * @brief Roles available for a device
 *
 */
typedef enum device_role { TRANSMITTER, RECEIVER } device_role;

/**
 * @brief Maximum size of an application layer packer
 *
 */
#define MAX_PACKET_SIZE 4092

/**
 * @brief Maximum size of the data field of an application layer packet
 *
 */
#define MAX_DATA_PER_PACKET_SIZE 4088

/**
 * @brief Open a data link on /dev/ttyS<port> with given role.
 *
 * @param port of /dev/ttyS<port>
 * @param role one of TRANSMITTER, RECEIVER
 * @return opened file descriptor, -1 in case of error
 */
int llopen(int port, device_role role);

/**
 * @brief Write packet with given length to a previously opened data link.
 *
 * @param fd data link file descriptor
 * @param buffer char character array to transmit
 * @param length buffer length
 * @return number of written chars, -1 in case of error
 */
int llwrite(int fd, unsigned char *buffer, int length);

/**
 * @brief Read packet from a previously opened data link to buffer.
 *
 * @param fd data link file descriptor
 * @param buffer output
 * @return number of read chars, -1 in case of error
 */
int llread(int fd, unsigned char *buffer);

/**
 * @brief Close a previously opened data link.
 *
 * @param fd data link file descriptor
 * @return -1 in case of error, 0 otherwise
 */
int llclose(int fd);

/**
 * @brief Get number of received information frame integrity errors (failed BCC2
 * or invalid headers). Only valid when called from a receiver.
 *
 * @return number of errors
 */
int llgeterrors();

/**
 * @brief Sets the approximate probability of frame errors
 *
 * @param probability Probability of an induced frame error
 */
void llsetinducedfer(int probability);

/**
 * @brief Sets the delay simulated propagation delay in the receiver when
 * receiving data through the llread function.
 *
 * @param delay_us Induced propagation delay
 */
void llsetinduceddelay(int delay_us);