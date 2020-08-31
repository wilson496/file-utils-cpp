/*
 * diskinfo.cpp
 * CSC 360 - Fall 2019 - P3 Part 1
 * 
 * Author: Cameron Wilson (V00822184)
 * Created on: 2019-11-13
 * 
 * Retrieve file system information from a disk image file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <fstream>
#include <iostream>


// lseek
#include <sys/types.h>
#include <unistd.h>

using namespace std;


struct __attribute__((__packed__)) superblock_t {
 uint8_t fs_id [8];
 uint16_t block_size;
 uint32_t file_system_block_count;
 uint32_t fat_start_block;
 uint32_t fat_block_count;
 uint32_t root_dir_start_block;
 uint32_t root_dir_block_count;
};

int main(int argc, char* argv[]) {

    if (argc != 2) {
        printf("You have provided %d arguments," 
               "which is an invald number of arguments.\n"
               "Refer to usage instructions below for how to use this program", argc);
        printf("Usage: disklist [IMAGE FILE]\n");
        return -1;
    }

    // Open a file with read/write permissions
    int fs;
    if ((fs = open(argv[1], O_RDWR)) == -1) {
        cout << "Failed to open file! Aborting program." << endl;
        return -1;
    }
    struct stat buffer;
    fstat(fs, &buffer);

    // Get address in disk image file via memory map
    // 0x01 = PROT_READ -> Pages may be read
    // 0x02 = PROT_WRITE -> Pages may be written
    // 0x001 = MAP_SHARED -> Share the map
    // For more information, consult the documentation: http://man7.org/linux/man-pages/man2/mmap.2.html
    char* address = (char*)mmap(NULL, buffer.st_size, 0x01 | 0x02, 0x001, fs, 0);

    // Check that the file system identifier matches 'CSC360FS'
    if (strncmp("CSC360FS", address, 8) != 0) {
        fprintf(stderr, "Error: incompatible disk image format.\n");
        return -1;
    }
    
    // Retrieve root directory info from disk image
    superblock_t superblock;
    memcpy(&superblock, address, sizeof(superblock));

    // Convert from Big Endian to Little Endian
    uint16_t blocksize = ntohs(superblock.block_size);
    uint32_t root_dir_start_block = ntohl(superblock.root_dir_start_block);
    uint32_t root_dir_block_count = ntohl(superblock.root_dir_block_count);
    uint32_t fat_start_block = ntohl(superblock.fat_start_block);
    uint32_t fat_block_count = ntohl(superblock.fat_block_count);
    uint32_t file_system_block_count = ntohl(superblock.file_system_block_count);
    
    
    //Assign converted values to struct
    superblock.block_size = blocksize;
    superblock.root_dir_start_block = root_dir_start_block;
    superblock.root_dir_block_count = root_dir_block_count; 
    superblock.fat_start_block = fat_start_block;
    superblock.fat_block_count = fat_block_count;
    superblock.file_system_block_count = file_system_block_count;
    

    // Calculate number of entries in FAT
    int numFATEntries;
    numFATEntries = (superblock.fat_block_count * superblock.block_size) 
                    / 4;

    // Allocate FAT entries
    uint32_t* fat;

    void* point = malloc(numFATEntries * sizeof(uint32_t));
    uint32_t* casted_point = (uint32_t*) point;

    if (!(fat = casted_point)) {
        fprintf(stderr, "An error has occured: %s", strerror(errno));
        return -1;
    }

    // Read FAT blocks
    int offset = 0;
    for (unsigned int i = superblock.fat_start_block; 
            i < superblock.fat_start_block + superblock.fat_block_count; 
            i++) {

        // Seek to block position (superblock is 0th block)
        int blockOffset = 512 + ((i - 1) * superblock.block_size);
        if (lseek(fs, blockOffset, SEEK_SET) != blockOffset) {
            perror("lseek error");
            return -1;
        }

        read(fs, (fat + offset), superblock.block_size);
        
        offset += superblock.block_size;

    }

    // Reverse bit endianness on all FAT entries
    for (int i = 0; i < numFATEntries; i++) {
        fat[i] = ntohl(fat[i]);
    }

    // Count number of free, allocated, reserved blocks
    int freeBlocks = 0, reservedBlocks = 0, allocatedBlocks = 0;
    for (int i = 0; i < numFATEntries; i++) {

        if (fat[i] == 0x00000000)
            freeBlocks++;
        else if (fat[i] == 0x00000001)
            reservedBlocks++;
        else
            allocatedBlocks++;

    }

    printf("Superblock information:\n"
           "Block size: %u\n"
           "Block count: %u\n"
           "FAT starts: %u\n"
           "FAT blocks: %u\n"
           "Root directory start: %u\n"
           "Root directory blocks: %u\n"
           "\n"
            "FAT information: \n"
            "Free Blocks: %u\n"
            "Reserved Blocks: %u\n"
            "Allocated Blocks: %u\n",
            superblock.block_size, superblock.file_system_block_count, superblock.fat_start_block, superblock.fat_block_count,
            superblock.root_dir_start_block, superblock.root_dir_block_count, freeBlocks, reservedBlocks, allocatedBlocks);

    return EXIT_SUCCESS;
}