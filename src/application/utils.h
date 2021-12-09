#pragma once

#include <time.h>

/**
 * @brief Prints the progress bar.
 *
 * @param curr_byte Current byte in the transfer.
 * @param file_size File size (in bytes)
 */
void print_transfer_progress_bar(unsigned curr_byte, unsigned file_size);

/**
 * @brief Elapsed time during the transfer.
 *
 * @param start struct timespec containing info about the tranfer start
 * @param end struct timespec containing info about the tranfer end
 * @return double Elapsed time during the tranfer (in seconds with a decimal
 * place)
 */
double elapsed_seconds(struct timespec *start, struct timespec *end);

/**
 * @brief Prints the content of the bytes in buf.
 *
 * @param buf Pointer to a buffer containing bytes
 * @param size Number of bytes to read
 */
void print_bytes(unsigned char *buf, int size);