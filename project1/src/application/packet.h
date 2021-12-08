#pragma once

#define MAX_FILENAME_LENGTH 1024  // Max filename lenght (only filename)
#define MAX_FILE_PATH_LENGTH 3072 // Max file path lenght (without filename)
#define MAX_SEQ_NO 256

/* CONTROL PACKET */
#define CP_CONTROL_START 2 // Control packet control field for start packets
#define CP_CONTROL_END 3   // Control packet control field for end packets
#define CP_TYPE_SIZE 0     // Control packet type field for total file size
#define CP_TYPE_FILENAME 1 // Control packet type field for filename
#define CP_TYPE_BYTES_PER_PACKET                                               \
    2 // Control packet type field for data bytes in each packet

/* DATA PACKET */
#define DP_CONTROL 1 // Data packet control field
#define DP_SEQ_NO(x) (x % MAX_SEQ_NO)

/**
 * @brief Assembles an application layer data packet based in the function
 * parameters
 *
 * @param out_packet Pointer to the assembled packet
 * @param seq_no Sequence number of the data in the packet (smaller than
 * MAX_SEQ_NO, use DP_SEQ_NO() to assure this)
 * @param data Pointer to the data to be included in the packet
 * @param data_size Number of data bytes to be included in the packet
 * @return int Size (in bytes) of the assembled data packet
 */
int assemble_data_packet(unsigned char *out_packet, unsigned char seq_no,
                         unsigned char *data, unsigned data_size);

/**
 * @brief Assembles an application layer control packet based in the function
 * parameters
 *
 * @param out_packet Pointer to the assembled packet
 * @param end Boolean flag that indicates if this is the end control packet
 * @param file_size Size of the file to be tranferred
 * @param bytes_per_packet Number of data bytes transported in each packet
 * @param file_name Name of the file to be transferred
 * @return int Size (in bytes) of the assembled control packet
 */
int assemble_control_packet(unsigned char *out_packet, int end,
                            unsigned file_size, unsigned bytes_per_packet,
                            char *file_name);