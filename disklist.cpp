/*
 * disklist.cpp
 * CSC 360 - Fall 2019 - P3 Part 2
 * 
 * Author: Cameron Wilson (V00822184)
 * Created on: 2019-11-16
 * 
 * List files for a given directory from a disk image's file system.
 * 
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
#include <vector>


using namespace std;

// Superblock struct
// Taken from tutorial slides
struct __attribute__((__packed__)) superblock_t {
 uint8_t fs_id [8];
 uint16_t block_size;
 uint32_t file_system_block_count;
 uint32_t fat_start_block;
 uint32_t fat_block_count;
 uint32_t root_dir_start_block;
 uint32_t root_dir_block_count;
};

// Date & Time entry for file/directory
// Taken from tutorial slides
struct __attribute__((__packed__)) dir_entry_timedate_t {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
};

// Directory entry
// Taken from tutorial slides
struct __attribute__((__packed__)) dir_entry_t {
    uint8_t                       status;
    uint32_t                      starting_block;
    uint32_t                      block_count;
    uint32_t                      size;
    struct   dir_entry_timedate_t modify_time;
    struct   dir_entry_timedate_t create_time;
    char                          filename[31];
    uint8_t                       unused[6];
};


#define FAT_ENTRY_SIZE 4


//lseek
#include <sys/stat.h>
#include <unistd.h>


int
ReadBlock(int diskfd, int blockNum, const superblock_t *superblock, 
            void* buf, int size) {


    uint32_t block_size = superblock->block_size;

    int superblock_size = 512;

    int block_size_num_product = ((blockNum - 1) * block_size);

    // Seek to block position (superblock is 0th block)
    int blockOffset = superblock_size + block_size_num_product;
    if (lseek(diskfd, blockOffset, SEEK_SET) != blockOffset) {
        cout << "lseek error. ReadBlock function has failed" << endl;
        return -1;
    }

    // Read block into buffer
    if (read(diskfd, buf, size) != size) {
        
        return -1;
    }

    return 0;

}


uint32_t *
ReadFAT(int diskfd, const superblock_t *superblock, int *numFATEntries) {

    // Calculate number of entries in FAT
    uint32_t fat_count = superblock->fat_block_count;
    uint32_t block_size = superblock->block_size;

    *numFATEntries = (fat_count * block_size) 
                    / FAT_ENTRY_SIZE;

    // Allocate FAT entries
    uint32_t* fat;
    if (!(fat = (uint32_t*) malloc(*numFATEntries * sizeof(uint32_t)))) {
        
        cout << "Error in allocating FAT entries!" << endl;
        // fprintf(stderr, "Error: %s", strerror(errno));
        return NULL;
    }

    // Read FAT blocks
    int offset = 0;
    for (int i = superblock->fat_start_block; 
            i < (int) superblock->fat_start_block + (int) superblock->fat_block_count; 
            i++) {

        ReadBlock(diskfd, i, superblock, fat + offset, block_size);
        offset += block_size;

    }

    // Reverse bit endianness on all FAT entries
    for (int i = 0; i < *numFATEntries; i++) {
        fat[i] = ntohl(fat[i]);
    }

    return fat;

}


dir_entry_t *
ReadDir(int dirBlock, int *numEntries, const superblock_t *superblock, int diskfd) {

    // Read directory block
    char dirbuf[superblock->block_size];

    if (ReadBlock(diskfd, dirBlock, superblock, dirbuf, 
                    superblock->block_size) != 0) {
        perror("Error reading directory block");
        return NULL;
    }

    int maxEntries = superblock->block_size / 64;

    dir_entry_t *dir_ent;
    if (!(dir_ent = (dir_entry_t*)malloc(maxEntries * sizeof(dir_entry_t)))) {
        cout << "Error: malloc. Cannot allocate space for directory entry. Abort from ReadDir function" << endl;
        return NULL;
    }

    // Copy directory entries
    int i;
    *numEntries = 0;  // number of entries recorded so far
    for (i = 0; i < maxEntries; i++) {

        dir_entry_t *file = &(dir_ent[*numEntries]);

        // Status byte
        char file_status = dirbuf[(i * 64)];

        if ((file_status & 0x01) == 0)                      // Entry unused
            continue;

        else if (file_status & 0x02)                        // Type is DIRECTORY
            file->status = 0;

        else if (file_status & 0x04)                        // Type is FILE
            file->status = 1;

        // Starting block (offset 1)
        memcpy(&(file->starting_block), dirbuf + (i * 64) + 1, 4);
        
        // Number of blocks (offset 5)
        memcpy(&file->block_count, dirbuf + (i * 64) + 5, 4);
        
        // File size (offset 9)
        memcpy(&file->size, dirbuf + (i * 64) + 9, 4);
    
        memcpy(&file->create_time, dirbuf + (i * 64) + 13, 7);
        
        memcpy(&file->modify_time, dirbuf + (i * 64) + 20, 7);

        // Convert to host byte order
        file->starting_block = ntohl(file->starting_block);
        file->size = ntohl(file->size);
        file->create_time.year = ntohs(file->create_time.year);
        file->modify_time.year = ntohs(file->modify_time.year);

        // File name (offset 27)
        strncpy(file->filename, (char*)dirbuf + (i * 64) + 27, 31);

        (*numEntries)++;

    }

    return dir_ent;

}




int main(int argc, char* argv[]) {
    
    if (argc != 2) {
        printf("You have provided %d arguments," 
               "which is an invald number of arguments.\n"
               "Refer to usage instructions below for how to use this program", argc);
        printf("\nUsage: disklist <IMAGE_FILE>\n");
        return -1;
    }


    // Open file
    int diskfd;
    if ((diskfd = open(argv[1], O_RDWR)) == -1) {
        cout << "Unable to open file. " << strerror(errno) << endl;
        return -1;
    }

    struct stat buffer;
    fstat(diskfd, &buffer);

    
    // Get address in disk image file via memory map
    // 0x01 = PROT_READ -> Pages may be read
    // 0x02 = PROT_WRITE -> Pages may be written
    // 0x001 = MAP_SHARED -> Share the map
    // For more information, consult the documentation: http://man7.org/linux/man-pages/man2/mmap.2.html
    char* address = (char*)mmap(NULL, buffer.st_size, 0x01 | 0x02, 0x001, diskfd, 0);

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


    // Read FAT
    int numFATEntries;
    uint32_t *fat = ReadFAT(diskfd, &superblock, &numFATEntries);

    // Print files in root directory
    while (true) {

        int numFiles;
        dir_entry_t *dir_ent = ReadDir(superblock.root_dir_start_block, &numFiles, &superblock, diskfd);

        for (int i = 0; i < numFiles; i++) {
            
            dir_entry_timedate_t modtime = dir_ent[i].modify_time;

            char type;
            if (dir_ent[i].status != 1){
                type = 'F';
            }
            else {
                type = 'D';
            }
                                     
            printf("%c ", type);
            printf("%10d ", dir_ent[i].size);
            printf("%30s ", dir_ent[i].filename);
            printf("%04u/%02u/%02u %02u:%02u:%02u\n", modtime.year, 
                                                            modtime.month,
                                                            modtime.day,
                                                            modtime.hour,
                                                            modtime.minute,
                                                            modtime.second);

        }

        free(dir_ent);

        if (fat[superblock.root_dir_start_block] == 0xFFFFFFFF) 
            break;
        else
            superblock.root_dir_start_block = fat[superblock.root_dir_start_block];
    }


    free(fat);
    close(diskfd);

    return 0;
    

}