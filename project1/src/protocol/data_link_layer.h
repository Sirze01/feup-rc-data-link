#pragma once

#include "frame.h"
#include <termios.h>

#define BAUDRATE B38400

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
int llwrite(int fd, char *buffer, int length);

/**
 * @brief Read packet from a previously opened data link to buffer.
 *
 * @param fd data link file descriptor
 * @param buffer
 * @return number of read chars, -1 in case of error
 */
int llread(int fd, char *buffer);

/**
 * @brief Close a previously opened data link.
 *
 * @param fd data link file descriptor
 * @return positive value in case of success, negative otherwise
 */
int llclose(int fd);
