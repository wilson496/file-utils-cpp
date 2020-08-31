/*
 * diskput.cpp
 * CSC 360 - Fall 2019 - P3 Part 4
 * 
 * Author: Cameron Wilson (V00822184)
 * Created on: 2019-11-25
 * 
 * Copy a file from your local machine, 
 * and then write it to a disk image's file system.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <fstream>
#include <iostream>
#include <vector>

#include <sstream>

// ceil
#include <cmath>
#include <ctgmath>



using namespace std;

//Superblock structure
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

#define FILENAME_MAXSIZE 31
#define SUPERBLOCK_SIZE 512  // bytes
#define FAT_ENTRY_SIZE 4


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
        // perror("read error");
        return -1;
    }

    return 0;

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

int
WriteBlock(int diskfd, int blockNum, const superblock_t *superblock, 
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

    // Write block to file
    if (write(diskfd, buf, size) != size) {
        perror("write error");
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


int
WriteDirectory(dir_entry_t *file, int dirBlock, uint32_t *fat, 
                const superblock_t *superblock, int diskfd) {

    // Check if file already exists - ERROR if it does

    dir_entry_t *files;
    int numFiles, entryOffset;
    uint16_t block_size = superblock->block_size;

    while (1) {
        if ((files = ReadDir(dirBlock, &numFiles, superblock, diskfd)) == NULL) {
            return -1;
        }

        for (int i = 0; i < numFiles; i++) {
            
            char* name = file->filename;

            if (strncmp(name, files[i].filename, 31) == 0) {
                cout << "Error: file " << name << " already exists\n" << endl;
                free(files);
                return -1;
            }

        }

        free(files);

        // Find an empty space for the new entry
        int i;
        char dirbuf[block_size];

        if (ReadBlock(diskfd, dirBlock, superblock, dirbuf, 
                        block_size) != 0) {
            return -1;
        }

        int maxEntries = block_size / 64;
        for (i = 0; i < maxEntries; i++) {
            
            entryOffset = i * 64;
            char status = dirbuf[entryOffset];

            // If 0th bit of status byte is 0, entry is available
            if ((status & 1) == 0)
                break;

        }

        if (i >= maxEntries) {
            
            if (fat[dirBlock] == 0xFFFFFFFF) {
                cout << "Error: Out of directory blocks!\n" << endl;
                return -1;

            }

            dirBlock = fat[dirBlock];
            continue;

        }
        else {
            break;
        }
    }
    

    
    // Write directory entry
    dir_entry_t file_to_write = *file;
    
    
    
    // Convert to host byte order 
    
    uint32_t starting_block = htonl(file_to_write.starting_block);
    file_to_write.starting_block = starting_block;
    
    uint32_t block_count = htonl(file_to_write.block_count);
    file_to_write.block_count = block_count;
    
    uint32_t size_of_file_uint32_t = htonl(file_to_write.size);
    file_to_write.size = size_of_file_uint32_t;

    uint16_t create_year = htons(file_to_write.create_time.year);
    file_to_write.create_time.year = create_year;
    
    uint16_t modify_year = htons(file_to_write.modify_time.year);
    file_to_write.modify_time.year = modify_year;
    

    // Assign values
    int current_block_offset = SUPERBLOCK_SIZE + block_size * (dirBlock - 1);
    int fileOffset = current_block_offset + entryOffset;

    if (lseek(diskfd, fileOffset, SEEK_SET) != fileOffset) {
        perror("lseek error");
        return -1;
    }

    if (write(diskfd, &file_to_write, sizeof(dir_entry_t)) != sizeof(dir_entry_t)) {
        perror("write error");
        return -1;
    }

    return 0;

}


int UpdateFAT(int diskfd, const superblock_t *superblock, int entryNum, 
            uint32_t val) {
    
    uint16_t block_size = superblock->block_size;

    uint32_t toWrite = htonl(val);  // convert to big-endian
    uint32_t fat_start_block = superblock->fat_start_block; 
    uint32_t fat_block_count = superblock->fat_block_count;
    int fat_start_offset_of_block = SUPERBLOCK_SIZE + superblock->block_size * (fat_start_block - 1);

    // Calculate offset from start of file in bytes
    int fileOffset = fat_start_offset_of_block + entryNum * sizeof(uint32_t);

    // Check entry within bounds of FAT table
    int fatEndBlock = fat_start_block + fat_block_count;  
    int fat_end_offset_of_block = SUPERBLOCK_SIZE + block_size * (fatEndBlock - 1);
    
    if (fileOffset >= fat_end_offset_of_block) {
        
        cout << "Out of bounds for Entry " << entryNum << "\n" << endl;
        return -1;
    }

    // Write new FAT entry
    if (lseek(diskfd, fileOffset, SEEK_SET) == -1) {
        
        cout << "lseek error" << endl;
        return -1;
    }

    if (write(diskfd, &toWrite, sizeof(uint32_t)) != sizeof(uint32_t)) {
        perror("FAT write error");
        return -1;
    }

    return 0;

}


int main(int argc, char* argv[]) {
    
    if (argc != 4) {
        printf("You have provided %d arguments," 
               "which is an invald number of arguments.\n"
               "Refer to usage instructions below for how to use this program", argc);
        printf("Usage: diskget <IMAGE FILE> <FILE IN FILE SYSTEM> <OUTPUT FILE NAME>\n");
        return -1;
    }

    // Open file
    int diskfd;
    if ((diskfd = open(argv[1], O_RDWR)) == -1) {
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
    size_t size_t_of_superblock = sizeof(superblock);
    memcpy(&superblock, address, size_t_of_superblock);

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

    
    int numFATEntries; // track number of FAT entries
    int inputfd; // file descriptor

    // Read FAT
    uint32_t *fat = ReadFAT(diskfd, &superblock, &numFATEntries);

    // Try to open input file
    if ((inputfd = open(argv[2], O_RDWR)) == -1) {

        if (errno == 2)
            cout << "File not found.\n" << endl;
        else
            cout << "Error opening input file" << endl;

        return -1;

    }

    // Get number of required blocks for file
    struct stat inputStat;
    if (fstat(inputfd, &inputStat) == -1)
        perror("fstat error");


    double ceil_result = ceil(inputStat.st_size / (double) superblock.block_size);
    int numReqdBlocks = (int) ceil_result;

    // Write blocks into disk
    unsigned int bytesTransferred = 0;
    unsigned int fatEntryNum = 0;
    unsigned int blockNum = 0;
    char fileBuf[superblock.block_size];
    uint32_t fileBlocks[numReqdBlocks];  // blocks allocated for file
    uint32_t file_size = inputStat.st_size;

    while (bytesTransferred < file_size) {
    

        // Calculate number of bytes to write from file
        int numBytes;

        int bytes_transferred_block_size_product = bytesTransferred / superblock.block_size;
        int file_size_block_size_product = file_size / superblock.block_size;

        if (bytes_transferred_block_size_product != file_size_block_size_product) {
            numBytes = superblock.block_size;
        }
        else {
            numBytes = file_size % superblock.block_size;
        }

        // Find free block to write into
        while (fat[fatEntryNum] != 0x00000000)
            fatEntryNum++;

        if (fatEntryNum >= (unsigned int)numFATEntries) {
            cout << "Error: Not enough space!\n" << endl;
            return -1;
        }

        // Read from input file and write into block
        if (read(inputfd, fileBuf, numBytes) != numBytes) {
            cout << "Error reading input file" << endl;
            return -1;
        }

        if (WriteBlock(diskfd, fatEntryNum, 
                        &superblock, fileBuf, numBytes) == -1) {
            cout << "Error writing to block" << endl;
            return -1;
        }

        fileBlocks[blockNum++] = fatEntryNum;
        bytesTransferred += numBytes;
        fatEntryNum++;

    }

    // Write Directory Entry

    dir_entry_t newfile;
    newfile.size = file_size;
    newfile.status = 0x03;
    newfile.block_count = numReqdBlocks;
    newfile.starting_block = fileBlocks[0];

    struct tm *lcltime1 = localtime(&inputStat.st_ctime);
    struct tm *lcltime2 = localtime(&inputStat.st_mtime);

    newfile.create_time.year = lcltime1->tm_year + 1900;
    newfile.create_time.day = lcltime1->tm_mday;
    newfile.create_time.month = lcltime1->tm_mon + 1;
    newfile.create_time.hour = lcltime1->tm_hour;
    newfile.create_time.minute = lcltime1->tm_min;
    newfile.create_time.second = lcltime1->tm_sec;
    
    newfile.modify_time.year = lcltime2->tm_year + 1900;
    newfile.modify_time.day = lcltime2->tm_mday;
    newfile.modify_time.month = lcltime2->tm_mon + 1;
    newfile.modify_time.hour = lcltime2->tm_hour;
    newfile.modify_time.minute = lcltime2->tm_min;
    newfile.modify_time.second = lcltime2->tm_sec;
    
    strncpy(newfile.filename, argv[3], FILENAME_MAXSIZE);

    int write_dir_result = WriteDirectory(&newfile, superblock.root_dir_start_block, fat, &superblock, diskfd);
    if (write_dir_result != 0) {
        return -1;
    }

    // Update FAT table (after directory entry, in case of error)
    for (int i = 1; i < numReqdBlocks; i++) {

        uint32_t fat_index = fileBlocks[i - 1];
        fat[fat_index] = fileBlocks[i];

        int update_fat_result = UpdateFAT(diskfd, &superblock, fileBlocks[i-1], fileBlocks[i]);
        if (update_fat_result == -1) {
            cout << "Error updating FAT" << endl;
            return -1;
        }

    }

    fat[fileBlocks[numReqdBlocks - 1]] = 0xFFFFFFFF;

    if (UpdateFAT(diskfd, &superblock, fileBlocks[numReqdBlocks - 1], 
                    0xFFFFFFFF) == -1) {
        perror("Error updating FAT");
        return -1;
    }

    free(fat);
    close(diskfd);
    close(inputfd);

    return 0;


}
