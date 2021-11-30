#pragma once

int read_validate_start_packet(int port_fd, char *out_file_name);

int write_file_from_stream(int port_fd, int fd);

int read_validate_end_packet(int port_fd);