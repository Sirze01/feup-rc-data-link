#pragma once

int read_validate_control_packet(int port_fd, char *out_file_name, int is_end);

int write_file_from_stream(int port_fd, int fd);

unsigned get_receiver_bytes_per_packet();