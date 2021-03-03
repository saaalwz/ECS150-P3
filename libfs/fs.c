#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

/* signature */
#define SIGNATURE "ECS150FS"
/* signature size */
#define SIGNATURE_SIZE 8
/* FAT EOC */
#define FAT_EOC 0xFFFF

#define MIN(x, y) ((x) < (y) ? (x) : (y))

typedef struct __attribute__ ((__packed__))
{
    char signature[SIGNATURE_SIZE];  /* Signature ECS150FS */
    uint16_t tblk_num;               /* Total amount of blocks of virtual disk */
    uint16_t root_idx;               /* Root directory block index */
    uint16_t dblk_idx;               /* Data block start index */
    uint16_t dblk_num;               /* Amount of data blocks */
    uint8_t  fblk_num;               /* Number of blocks for FAT */
    uint8_t  padding[4079];          /* Unused/Padding */
} SuperBlock;

typedef struct __attribute__ ((__packed__))
{
    char name[FS_FILENAME_LEN];  /* Filename (including NULL character) */
    uint32_t size;               /* Size of the file (in bytes) */
    uint16_t dblk_idx;           /* Index of the first data block */
    uint8_t padding[10];         /* Unused/Padding */
} File;

typedef struct
{
    File* file;     /* file pointer to root */
    size_t offset;  /* file offset */
} Dirent;

/* global variables */
SuperBlock super_block;            /* super block */
File root[FS_FILE_MAX_COUNT];      /* root directory */
uint16_t* fat = NULL;              /* fat array */
Dirent opened[FS_OPEN_MAX_COUNT];  /* already opened files */

/* TODO: Phase 1 */

int fs_mount(const char *diskname)
{
    /* TODO: Phase 1 */
    int i;

    /* open disk file */
    if (block_disk_open(diskname) < 0) {
        return -1;
    }

    /* read super block */
    if (block_read(0, &super_block) < 0) {
        return -1;
    }

    /* check signature */
    if (memcmp(super_block.signature, SIGNATURE, SIGNATURE_SIZE) != 0) {
        return -1;
    }

    /* check total block number */
    if (super_block.tblk_num != block_disk_count()) {
        return -1;
    }

    /* malloc FAT */
    fat = malloc(super_block.fblk_num * BLOCK_SIZE);
    if (!fat) {
        return -1;
    }

    /* read FAT blocks */
    for (i = 0; i < super_block.fblk_num; ++i) {
        if (block_read(i + 1, &fat[(i * BLOCK_SIZE) / 2]) < 0) {
            free(fat);
            return -1;
        }
    }

    /* read root directory */
    if (block_read(super_block.root_idx, &root) < 0) {
        free(fat);
        return -1;
    }

    memset(&opened, 0, FS_OPEN_MAX_COUNT * sizeof(Dirent));

    /* success */
    return 0;
}

int fs_umount(void)
{
    /* TODO: Phase 1 */
    int i;

    /* check whether mounted */
    if (!fat) {
        return -1;
    }

    /* write FAT to disk file */
    for (i = 0; i < super_block.fblk_num; ++i) {
        if (block_write(1 + i, &fat[(i * BLOCK_SIZE) / 2]) < 0) {
            free(fat);
            return -1;
        }
    }
    free(fat);
    fat = NULL;

    
    /* write root directory */
	if (block_write(super_block.root_idx, &root) < 0) {
	    return -1;
	}

	/* close disk */
	return block_disk_close();
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	int i;
	int free_files = 0;
	int free_blocks = 0;

	/* calculate file number */
    for (i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (root[i].name[0] == '\0') {
            ++free_files;
        }
    }

    /* calculate free data block number */
    for (i = 0; i < super_block.dblk_num; ++i) {
        if (fat[i] == 0) {
            ++free_blocks;
        }
    }

    /* print out fs info */
    printf("FS Info:\n");
    printf("total_blk_count=%d\n", super_block.tblk_num);
    printf("fat_blk_count=%d\n", super_block.fblk_num);
    printf("rdir_blk=%d\n", super_block.root_idx);
    printf("data_blk=%d\n", super_block.dblk_idx);
    printf("data_blk_count=%d\n", super_block.dblk_num);
    printf("fat_free_ratio=%d/%d\n", free_blocks, super_block.dblk_num);
    printf("rdir_free_ratio=%d/%d\n", free_files, FS_FILE_MAX_COUNT);

    return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
	int i;
    File* file = NULL;

    /* check file system  */
    if (!fat) {
        return -1;
    }

    /* check filename length */
	if (strlen(filename) >= FS_FILENAME_LEN || strlen(filename) == 0) {
	    return -1;
	}

	/* check whether already exists */
    for (i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (strcmp(root[i].name, filename) == 0) {
            return -1;
        }
    }

	/* find and empty entry */
	for (i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (root[i].name[0] == '\0') {
            file = &root[i];
            break;
        }
	}

	/* root is full */
	if (!file) {
        return -1;
	}

	/* create file */
	strcpy(file->name, filename);
	file->size = 0;
	file->dblk_idx = FAT_EOC;

	return 0;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
    int i;
    File* file = NULL;
    uint16_t block;
    uint16_t temp;

    /* check file system  */
    if (!fat) {
        return -1;
    }

    /* check filename length */
    if (strlen(filename) >= FS_FILENAME_LEN || strlen(filename) == 0) {
        return -1;
    }

    /* check whether already exists */
    for (i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (strcmp(root[i].name, filename) == 0) {
            file = &root[i];
            break;
        }
    }
    if (!file) {
        return -1;
    }

    /* check whether opened */
    for (i = 0; i < FS_OPEN_MAX_COUNT; ++i) {
        if (opened[i].file == file) {
            return -1;
        }
    }

    /* delete all data block */
    block = file->dblk_idx;
    while (block != FAT_EOC) {
        temp = block;
        /* get next block */
        block = fat[block];
        /* free old block */
        fat[temp] = 0;
    }

    /* clear size and name */
    file->dblk_idx = FAT_EOC;
    file->size = 0;
    file->name[0] = '\0';

    return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	int i;

    /* check file system  */
    if (!fat) {
        return -1;
    }

    /* ls all files */
    printf("FS Ls:\n");
    for (i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        /* skip empty */
        if (root[i].name[0] == '\0') {
            continue;
        }

        /* print file info */
        printf("file: %s, size: %d, data_blk: %d\n",
               root[i].name, root[i].size, root[i].dblk_idx);
    }

    return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	int i;
	File* file = NULL;

    /* check file system  */
    if (!fat) {
        return -1;
    }

    /* check filename length */
    if (strlen(filename) >= FS_FILENAME_LEN || strlen(filename) == 0) {
        return -1;
    }

    /* check whether already exists */
    for (i = 0; i < FS_FILE_MAX_COUNT; ++i) {
        if (strcmp(root[i].name, filename) == 0) {
            file = &root[i];
            break;
        }
    }
    if (!file) {
        return -1;
    }

    /* find an empty dirent */
    for (i = 0; i < FS_OPEN_MAX_COUNT; ++i) {
        /* found */
        if (!opened[i].file) {
            break;
        }
    }

    /* no empty dirent */
    if (i >= FS_OPEN_MAX_COUNT) {
        return -1;
    }

    /* set opened dirent */
    opened[i].file = file;
    opened[i].offset = 0;

    return i;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */

	/* check file system  */
    if (!fat) {
        return -1;
    }

    /* check fd */
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
        return -1;
    }

    /* not opened */
    if (!opened[fd].file) {
        return -1;
    }

    /* clear dirent */
    opened[fd].file = NULL;
    opened[fd].offset = 0;

    return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */

    /* check file system  */
    if (!fat) {
        return -1;
    }

    /* check fd */
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
        return -1;
    }

    /* not opened */
    if (!opened[fd].file) {
        return -1;
    }

    /* return file size */
    return opened[fd].file->size;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
    /* check file system  */
    if (!fat) {
        return -1;
    }

    /* check fd */
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
        return -1;
    }

    /* not opened */
    if (!opened[fd].file) {
        return -1;
    }

    /* check offset */
    if (offset > opened[fd].file->size) {
        return -1;
    }

    /* set offset */
    opened[fd].offset = offset;
    return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	File* file;
	Dirent* dirent;
	uint32_t left_size;
	uint32_t alloc_size;
    uint32_t alloc_num;
	uint32_t block_num;
	size_t written;
	size_t n;
	uint16_t block;
	uint16_t prev;
	int i;
	uint32_t start_block;
	uint32_t start_pos;
	char block_buf[BLOCK_SIZE];

	/* check file system  */
    if (!fat) {
        return -1;
    }

    /* check fd */
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
        return -1;
    }

    /* not opened */
    if (!opened[fd].file) {
        return -1;
    }

    /* check buf */
    if (!buf) {
        return -1;
    }

    /* get dirent and file */
    dirent = &opened[fd];
    file = dirent->file;

    /* find block number of file */
    block_num = 0;
    block = file->dblk_idx;
    prev = block;
    while (block != FAT_EOC) {
        ++block_num;
        prev = block;
        block = fat[block];
    }

    /* check whether need to alloc block */
    left_size = block_num * BLOCK_SIZE - dirent->offset;
    alloc_size = 0;
    /* calculate how much bytes need to alloc */
    if (count > left_size) {
        alloc_size = count - left_size;
    }

    /* alloc block if needed */
    if (alloc_size > 0) {
        /* calculate block number need to alloc */
        alloc_num = (alloc_size - 1) / BLOCK_SIZE + 1;

        /* iterate FAT */
        for (i = 0; i < super_block.dblk_num; ++i) {
            /* all allocated, done */
            if (alloc_num == 0) {
                break;
            }

            /* find a free data block */
            if (fat[i] == 0) {
                /* record total block number */
                ++block_num;
                /* decrease needed */
                --alloc_num;

                /* alloc the block */
                if (prev == FAT_EOC) {
                    file->dblk_idx = i;
                } else {
                    fat[prev] = i;
                }
                prev = i;
            }
        }
        /* set tail of chain */
        fat[prev] = FAT_EOC;
    }

    /* calculate start block and pos */
    start_block = file->dblk_idx;
    for (i = 0; i < dirent->offset / BLOCK_SIZE; ++i) {
        start_block = fat[start_block];
    }
    start_pos = dirent->offset % BLOCK_SIZE;

    /* start writing */
    written = 0;
    while (written < count && start_block != FAT_EOC) {
        /* read in the block to write */
        if (block_read(start_block, block_buf) < 0) {
            return -1;
        }
        /* write */
        n = MIN(BLOCK_SIZE - start_pos, count - written);
        memcpy(&block_buf[start_pos], &((char*) buf)[written], n);
        /* write block back to file  */
        if (block_write(start_block, block_buf) < 0) {
            return -1;
        }

        /* get next block to write */
        start_block = fat[start_block];
        /* reset pos */
        start_pos = 0;
        /* record written bytes */
        written += n;
    }

    /* update file size if needed */
    if (dirent->offset + written > file->size) {
        file->size = dirent->offset + written;
    }
    /* update offset */
    dirent->offset += written;

    return (int) written;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
	File* file;
	Dirent* dirent;
	size_t readn;
	size_t n;
    int i;
    uint32_t start_block;
    uint32_t start_pos;
    char block_buf[BLOCK_SIZE];

    /* check file system  */
    if (!fat) {
        return -1;
    }

    /* check fd */
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT) {
        return -1;
    }

    /* not opened */
    if (!opened[fd].file) {
        return -1;
    }

    /* check buf */
    if (!buf) {
        return -1;
    }

    /* get the file and dirent */
    dirent = &opened[fd];
    file = dirent->file;

    /* calculate start block and pos */
    start_block = file->dblk_idx;
    for (i = 0; i < dirent->offset / BLOCK_SIZE; ++i) {
        start_block = fat[start_block];
    }
    start_pos = dirent->offset % BLOCK_SIZE;

    /* start reading */
    readn = 0;
    while (readn < count && start_block != FAT_EOC) {
        /* read data block */
        if (block_read(start_block, block_buf) < 0) {
            return -1;
        }
        /* copy data to buffer */
        n = MIN(BLOCK_SIZE - start_pos, count - readn);
        memcpy(&((char*)buf)[readn], &block_buf[start_pos], n);

        /* get next block to write */
        start_block = fat[start_block];
        /* reset pos */
        start_pos = 0;
        /* record read bytes */
        readn += n;
    }

    /* update offset */
    dirent->offset += readn;
    /* return read num */
    return (int) readn;
}
