#pragma once

/**
 * @brief Parses a complete file path to an packet filename field sring. If no
 * filename provided gives the name of the file to send.
 *
 * @param out_packet_file_name Pointer to an empty string buffer
 * @param file_name Name of the file to send
 * @param file_path Path to the file to send
 */
void assemble_packet_file_name(char *out_packet_file_name, char *file_name,
                               char *file_path);

/**
 * @brief Sends a control packet, assembling the control packet and sending it.
 *
 * @param port_fd Port file descriptor
 * @param file_size Size of the file to be tranferred
 * @param bytes_per_packet Number of data bytes transported in each packet
 * @param packet_file_name Name of the file to be transferred
 * @param is_end Boolean flag that indicates if this is the end control packet
 * @return int -1 in case of error, 0 otherwise
 */
int send_control_packet(int port_fd, unsigned file_size, int bytes_per_packet,
                        char *packet_file_name, int is_end);

/**
 * @brief Sends a file, reading data from file, assembling the data packet and
 * sending it and updating the upload progress bar.
 *
 * @param port_fd Port file descriptor
 * @param fd File to send file descriptor
 * @param bytes_per_packet Number of data bytes transported in each packet
 * @param file_size Size of the file to be tranferred
 * @return int -1 in case of error, 0 otherwise
 */
int send_file_data(int port_fd, int fd, int bytes_per_packet, int file_size);