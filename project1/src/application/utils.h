#pragma once

#include <time.h>

void print_progress_bar(unsigned curr_byte, unsigned file_size);

double elapsed_seconds(struct timespec *start, struct timespec *end);