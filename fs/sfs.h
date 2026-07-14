#ifndef SFS_H
#define SFS_H

#include "types.h"

#define SFS_MAGIC       0x53465321

#define SFS_FILE        0
#define SFS_DIR         1

#define MAX_FILENAME    28
#define SFS_DIRECT_BLOCKS   12
#define MAX_FILES       16
#define ROOT_INO        1

struct super_block {
    u32 magic;
    u32 total_blocks;
    u32 inode_count;
    u32 inode_bitmap_start;
    u32 block_bitmap_start;
    u32 inode_table_start;
    u32 data_block_start;
} __attribute__((packed));

struct sfs_inode {
    u32 ino;
    u32 size;
    u8  type;
    u32 block_count;
    u32 blocks[SFS_DIRECT_BLOCKS];
} __attribute__((packed));

struct dentry {
    u32 ino;
    char name[MAX_FILENAME];
} __attribute__((packed));

struct file {
    u32 ino;
    u32 offset;
    u8  mode;
    u8  in_use;
};

void sfs_format(void);
int  sfs_mount(void);

int  sfs_alloc_inode(void);
void sfs_free_inode(u32 ino);
int  sfs_read_inode(u32 ino, struct sfs_inode *inode);
int  sfs_write_inode(struct sfs_inode *inode);

int  sfs_alloc_block(void);
void sfs_free_block(u32 block_no);

int  sfs_lookup(u32 dir_ino, const char *name);
int  sfs_create(u32 parent_ino, const char *name, u8 type);
int  sfs_read_file(struct sfs_inode *inode, u8 *buf, u32 size, u32 offset);
int  sfs_write_file(struct sfs_inode *inode, const u8 *buf, u32 size, u32 offset);

int  sys_open(const char *path);
int  sys_read(int fd, char *buf, int count);
int  sys_write(int fd, const char *buf, int count);
void sys_close(int fd);

void sfs_dump(void);

#endif
