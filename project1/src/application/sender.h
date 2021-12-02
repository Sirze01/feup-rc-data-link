#pragma once

void assemble_packet_file_name(char *out_packet_file_name, char* file_name, char* file_path);

int send_control_packet(int port_fd, unsigned file_size, int bytes_per_packet,
                        char *packet_file_name, int is_end);

int send_file_data(int port_fd, int fd, int bytes_per_packet, int file_size);