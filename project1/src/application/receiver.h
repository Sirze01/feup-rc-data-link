#pragma once

/**
 * @brief Reads a control packet, validating it against expected parameters and,
 * in case of the start packet, setting the needed global variables.
 *
 * @param port_fd Port file descriptor
 * @param out_file_name Filename expected
 * @param is_end Boolean flag indicating if this control packet is expected to
 * the the end packet
 * @return int -1 in case of bad incorredct data transmission or unsupported
 * parameters, -2 in case of not being the expected packet (not being the
 * expected (start/end) control packet or being a data packet)
 */
int read_validate_control_packet(int port_fd, char *out_file_name, int is_end);

/**
 * @brief Receives a file, reading data packets, validating its control fields,
 * writing the their data field contents to a file and updating the download
 * progress bar.
 *
 * @param port_fd Port file descriptor
 * @param fd File descriptor
 * @return int -1 in case of error, 0 otherwise
 */
int write_file_from_stream(int port_fd, int fd);

/**
 * @brief Get the receiver bytes per packet object.
 *
 * @return unsigned Number of bytes per packet
 */
unsigned get_receiver_bytes_per_packet();