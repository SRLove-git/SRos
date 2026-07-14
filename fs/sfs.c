#include "sfs.h"
#include "ramdisk.h"
#include "string.h"
#include "serial.h"

#define INODE_BITMAP_BLOCK   1
#define BLOCK_BITMAP_BLOCK   2
#define INODE_TABLE_START    3
#define INODE_TABLE_BLOCKS   2
#define DATA_BLOCK_START     5

#define INODES_PER_BLOCK     (BLOCK_SIZE / sizeof(struct sfs_inode))
#define MAX_INODES           (INODE_TABLE_BLOCKS * INODES_PER_BLOCK)

static struct super_block sb;
static int mounted = 0;
static struct file file_table[MAX_FILES];

static int bitmap_find_first_zero(u8 *bitmap, int max_bits)
{
    for (int i = 0; i < max_bits; i++)
        if (!(bitmap[i / 8] & (1 << (i % 8))))
            return i;
    return -1;
}

static void bitmap_set(u8 *bitmap, int idx)
{
    bitmap[idx / 8] |= (1 << (idx % 8));
}

static void bitmap_clear(u8 *bitmap, int idx)
{
    bitmap[idx / 8] &= ~(1 << (idx % 8));
}

static const char *path_parse(const char *path, u32 *parent_ino_out)
{
    if (!path || path[0] == '\0') {
        *parent_ino_out = ROOT_INO;
        return NULL;
    }
    while (*path == '/') path++;
    if (*path == '\0') {
        *parent_ino_out = ROOT_INO;
        return NULL;
    }
    *parent_ino_out = ROOT_INO;
    return path;
}

/* ===== 格式化 & 挂载 ===== */

void sfs_format(void)
{
    u8 block[BLOCK_SIZE];
    serial_printf(SERIAL_COM1, "[sfs] Formatting...\n");

    memset(&sb, 0, sizeof(sb));
    sb.magic              = SFS_MAGIC;
    sb.total_blocks       = DISK_BLOCKS;
    sb.inode_count        = MAX_INODES;
    sb.inode_bitmap_start = INODE_BITMAP_BLOCK;
    sb.block_bitmap_start = BLOCK_BITMAP_BLOCK;
    sb.inode_table_start  = INODE_TABLE_START;
    sb.data_block_start   = DATA_BLOCK_START;

    memset(block, 0, BLOCK_SIZE);
    for (u32 i = 0; i < sizeof(struct super_block); i++)
        block[i] = ((u8 *)&sb)[i];
    block_write(0, block);

    memset(block, 0, BLOCK_SIZE);
    bitmap_set(block, 0);
    block_write(INODE_BITMAP_BLOCK, block);

    memset(block, 0, BLOCK_SIZE);
    for (u32 i = 0; i < DATA_BLOCK_START; i++)
        bitmap_set(block, i);
    block_write(BLOCK_BITMAP_BLOCK, block);

    memset(block, 0, BLOCK_SIZE);
    block_write(INODE_TABLE_START, block);
    block_write(INODE_TABLE_START + 1, block);

    struct sfs_inode root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.ino  = ROOT_INO;
    root_inode.type = SFS_DIR;
    root_inode.size = 0;
    root_inode.block_count = 0;

    int root_block = sfs_alloc_block();
    root_inode.blocks[0] = root_block;
    root_inode.block_count = 1;

    struct dentry dentries[16];
    memset(dentries, 0, sizeof(dentries));
    dentries[0].ino = ROOT_INO;
    strcpy(dentries[0].name, ".");
    dentries[1].ino = ROOT_INO;
    strcpy(dentries[1].name, "..");

    memset(block, 0, BLOCK_SIZE);
    for (u32 i = 0; i < sizeof(dentries); i++)
        block[i] = ((u8 *)dentries)[i];
    block_write(root_block, block);

    root_inode.size = 2 * sizeof(struct dentry);
    sfs_write_inode(&root_inode);

    block_read(INODE_BITMAP_BLOCK, block);
    bitmap_set(block, ROOT_INO);
    block_write(INODE_BITMAP_BLOCK, block);

    serial_printf(SERIAL_COM1, "[sfs] Format complete.\n");
}

int sfs_mount(void)
{
    u8 block[BLOCK_SIZE];
    serial_printf(SERIAL_COM1, "[sfs] Mounting...\n");

    block_read(0, block);
    for (u32 i = 0; i < sizeof(struct super_block); i++)
        ((u8 *)&sb)[i] = block[i];

    if (sb.magic != SFS_MAGIC) {
        serial_printf(SERIAL_COM1, "[sfs] ERROR: bad magic 0x%x\n", sb.magic);
        return -1;
    }

    for (int i = 0; i < MAX_FILES; i++)
        file_table[i].in_use = 0;

    mounted = 1;
    serial_printf(SERIAL_COM1, "[sfs] Mount OK: %d inodes, %d data blocks\n",
                  sb.inode_count, sb.total_blocks - sb.data_block_start);
    return 0;
}

/* ===== inode 操作 ===== */

int sfs_alloc_inode(void)
{
    u8 bitmap[BLOCK_SIZE];
    block_read(sb.inode_bitmap_start, bitmap);
    int ino = bitmap_find_first_zero(bitmap, sb.inode_count);
    if (ino < 0) return -1;
    bitmap_set(bitmap, ino);
    block_write(sb.inode_bitmap_start, bitmap);
    return ino;
}

void sfs_free_inode(u32 ino)
{
    if (ino == 0 || ino >= sb.inode_count) return;
    u8 bitmap[BLOCK_SIZE];
    block_read(sb.inode_bitmap_start, bitmap);
    bitmap_clear(bitmap, ino);
    block_write(sb.inode_bitmap_start, bitmap);
}

int sfs_read_inode(u32 ino, struct sfs_inode *inode)
{
    if (ino == 0 || ino >= sb.inode_count) return -1;
    u32 index = ino - 1;
    u32 block_no = sb.inode_table_start + (index / INODES_PER_BLOCK);
    u32 offset   = (index % INODES_PER_BLOCK) * sizeof(struct sfs_inode);
    u8 buf[BLOCK_SIZE];
    block_read(block_no, buf);
    for (u32 i = 0; i < sizeof(struct sfs_inode); i++)
        ((u8 *)inode)[i] = buf[offset + i];
    return 0;
}

int sfs_write_inode(struct sfs_inode *inode)
{
    if (inode->ino == 0 || inode->ino >= sb.inode_count) return -1;
    u32 index = inode->ino - 1;
    u32 block_no = sb.inode_table_start + (index / INODES_PER_BLOCK);
    u32 offset   = (index % INODES_PER_BLOCK) * sizeof(struct sfs_inode);
    u8 buf[BLOCK_SIZE];
    block_read(block_no, buf);
    for (u32 i = 0; i < sizeof(struct sfs_inode); i++)
        buf[offset + i] = ((u8 *)inode)[i];
    block_write(block_no, buf);
    return 0;
}

/* ===== 数据块操作 ===== */

int sfs_alloc_block(void)
{
    u8 bitmap[BLOCK_SIZE];
    block_read(sb.block_bitmap_start, bitmap);
    int idx = bitmap_find_first_zero(bitmap, sb.total_blocks);
    if (idx < 0 || idx < (int)sb.data_block_start) return -1;
    bitmap_set(bitmap, idx);
    block_write(sb.block_bitmap_start, bitmap);
    return idx;
}

void sfs_free_block(u32 block_no)
{
    if (block_no < sb.data_block_start || block_no >= sb.total_blocks) return;
    u8 bitmap[BLOCK_SIZE];
    block_read(sb.block_bitmap_start, bitmap);
    bitmap_clear(bitmap, block_no);
    block_write(sb.block_bitmap_start, bitmap);
}

/* ===== 目录 & 文件操作 ===== */

int sfs_lookup(u32 dir_ino, const char *name)
{
    struct sfs_inode dir_inode;
    if (sfs_read_inode(dir_ino, &dir_inode) < 0) return -1;
    if (dir_inode.type != SFS_DIR) return -1;

    u8 block[BLOCK_SIZE];
    struct dentry d;
    for (u32 bi = 0; bi < dir_inode.block_count; bi++) {
        block_read(dir_inode.blocks[bi], block);
        u32 dentries_per_block = BLOCK_SIZE / sizeof(struct dentry);
        for (u32 di = 0; di < dentries_per_block; di++) {
            u32 off = di * sizeof(struct dentry);
            for (u32 i = 0; i < sizeof(struct dentry); i++)
                ((u8 *)&d)[i] = block[off + i];
            if (d.ino != 0 && strcmp(d.name, name) == 0)
                return d.ino;
        }
    }
    return -1;
}

int sfs_create(u32 parent_ino, const char *name, u8 type)
{
    struct sfs_inode parent_inode;
    if (sfs_read_inode(parent_ino, &parent_inode) < 0) return -1;
    if (parent_inode.type != SFS_DIR) return -1;
    if (sfs_lookup(parent_ino, name) >= 0) return -1;

    int new_ino = sfs_alloc_inode();
    if (new_ino < 0) return -1;

    struct sfs_inode new_inode;
    memset(&new_inode, 0, sizeof(new_inode));
    new_inode.ino  = new_ino;
    new_inode.type = type;

    if (type == SFS_DIR) {
        int block_no = sfs_alloc_block();
        if (block_no < 0) { sfs_free_inode(new_ino); return -1; }
        new_inode.blocks[0] = block_no;
        new_inode.block_count = 1;
        struct dentry dentries[16];
        memset(dentries, 0, sizeof(dentries));
        dentries[0].ino = new_ino;
        strcpy(dentries[0].name, ".");
        dentries[1].ino = parent_ino;
        strcpy(dentries[1].name, "..");
        u8 block[BLOCK_SIZE];
        memset(block, 0, BLOCK_SIZE);
        for (u32 i = 0; i < sizeof(dentries); i++)
            block[i] = ((u8 *)dentries)[i];
        block_write(block_no, block);
        new_inode.size = 2 * sizeof(struct dentry);
    }

    sfs_write_inode(&new_inode);

    {
        u8 block[BLOCK_SIZE];
        struct dentry d;
        int written = 0;

        for (u32 bi = 0; bi < parent_inode.block_count && !written; bi++) {
            block_read(parent_inode.blocks[bi], block);
            u32 dentries_per_block = BLOCK_SIZE / sizeof(struct dentry);
            for (u32 di = 0; di < dentries_per_block && !written; di++) {
                u32 off = di * sizeof(struct dentry);
                for (u32 i = 0; i < sizeof(struct dentry); i++)
                    ((u8 *)&d)[i] = block[off + i];
                if (d.ino == 0) {
                    d.ino = new_ino;
                    for (u32 i = 0; name[i] && i < MAX_FILENAME - 1; i++)
                        d.name[i] = name[i];
                    d.name[MAX_FILENAME - 1] = '\0';
                    for (u32 i = 0; i < sizeof(struct dentry); i++)
                        block[off + i] = ((u8 *)&d)[i];
                    block_write(parent_inode.blocks[bi], block);
                    written = 1;
                }
            }
        }

        if (!written) {
            if (parent_inode.block_count >= SFS_DIRECT_BLOCKS) {
                sfs_free_inode(new_ino);
                return -1;
            }
            int new_block = sfs_alloc_block();
            if (new_block < 0) { sfs_free_inode(new_ino); return -1; }
            memset(block, 0, BLOCK_SIZE);
            d.ino = new_ino;
            for (u32 i = 0; name[i] && i < MAX_FILENAME - 1; i++)
                d.name[i] = name[i];
            d.name[MAX_FILENAME - 1] = '\0';
            for (u32 i = 0; i < sizeof(struct dentry); i++)
                block[i] = ((u8 *)&d)[i];
            block_write(new_block, block);
            parent_inode.blocks[parent_inode.block_count++] = new_block;
        }

        parent_inode.size += sizeof(struct dentry);
        sfs_write_inode(&parent_inode);
    }

    serial_printf(SERIAL_COM1, "[sfs] create '%s' -> ino %d\n", name, new_ino);
    return new_ino;
}

int sfs_read_file(struct sfs_inode *inode, u8 *buf, u32 size, u32 offset)
{
    if (offset >= inode->size) return 0;
    if (offset + size > inode->size) size = inode->size - offset;

    u32 bytes_read = 0;
    u8 block[BLOCK_SIZE];

    while (bytes_read < size) {
        u32 current_offset = offset + bytes_read;
        u32 block_idx = current_offset / BLOCK_SIZE;
        u32 block_off = current_offset % BLOCK_SIZE;
        if (block_idx >= inode->block_count) break;
        block_read(inode->blocks[block_idx], block);
        u32 to_copy = BLOCK_SIZE - block_off;
        if (to_copy > size - bytes_read) to_copy = size - bytes_read;
        for (u32 i = 0; i < to_copy; i++)
            buf[bytes_read + i] = block[block_off + i];
        bytes_read += to_copy;
    }
    return bytes_read;
}

int sfs_write_file(struct sfs_inode *inode, const u8 *buf, u32 size, u32 offset)
{
    u32 bytes_written = 0;
    u8 block[BLOCK_SIZE];

    while (bytes_written < size) {
        u32 current_offset = offset + bytes_written;
        u32 block_idx = current_offset / BLOCK_SIZE;
        u32 block_off = current_offset % BLOCK_SIZE;

        if (block_idx >= inode->block_count) {
            if (block_idx >= SFS_DIRECT_BLOCKS) break;
            int new_block = sfs_alloc_block();
            if (new_block < 0) break;
            inode->blocks[block_idx] = new_block;
            inode->block_count++;
        }

        block_read(inode->blocks[block_idx], block);
        u32 to_copy = BLOCK_SIZE - block_off;
        if (to_copy > size - bytes_written) to_copy = size - bytes_written;
        for (u32 i = 0; i < to_copy; i++)
            block[block_off + i] = buf[bytes_written + i];
        block_write(inode->blocks[block_idx], block);
        bytes_written += to_copy;
    }

    if (offset + bytes_written > inode->size)
        inode->size = offset + bytes_written;
    return bytes_written;
}

/* ===== 系统调用接口 ===== */

int sys_open(const char *path)
{
    if (!mounted) return -1;
    u32 parent_ino;
    const char *name = path_parse(path, &parent_ino);
    if (name == NULL) return -1;

    int ino = sfs_lookup(parent_ino, name);
    if (ino < 0) return -1;

    int fd = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!file_table[i].in_use) { fd = i; break; }
    }
    if (fd < 0) return -1;

    file_table[fd].ino    = ino;
    file_table[fd].offset = 0;
    file_table[fd].mode   = 2;
    file_table[fd].in_use = 1;
    return fd;
}

int sys_read(int fd, char *buf, int count)
{
    if (fd < 0 || fd >= MAX_FILES || !file_table[fd].in_use) return -1;
    struct sfs_inode inode;
    if (sfs_read_inode(file_table[fd].ino, &inode) < 0) return -1;
    int n = sfs_read_file(&inode, (u8 *)buf, count, file_table[fd].offset);
    file_table[fd].offset += n;
    return n;
}

int sys_write(int fd, const char *buf, int count)
{
    if (fd < 0 || fd >= MAX_FILES || !file_table[fd].in_use) return -1;
    struct sfs_inode inode;
    if (sfs_read_inode(file_table[fd].ino, &inode) < 0) return -1;
    int n = sfs_write_file(&inode, (const u8 *)buf, count, file_table[fd].offset);
    file_table[fd].offset += n;
    sfs_write_inode(&inode);
    return n;
}

void sys_close(int fd)
{
    if (fd < 0 || fd >= MAX_FILES) return;
    file_table[fd].in_use = 0;
}

void sfs_dump(void)
{
    serial_printf(SERIAL_COM1, "\n=== SFS Dump ===\n");
    if (!mounted) {
        serial_printf(SERIAL_COM1, "  (not mounted)\n== end ==\n");
        return;
    }

    serial_printf(SERIAL_COM1, "Super Block: magic=0x%x, total_blocks=%d\n",
                  sb.magic, sb.total_blocks);
    serial_printf(SERIAL_COM1, "  inodes=%d, data_blocks=%d~%d\n",
                  sb.inode_count, sb.data_block_start, sb.total_blocks - 1);

    serial_printf(SERIAL_COM1, "\nRoot directory:\n");
    {
        u8 block[BLOCK_SIZE];
        struct sfs_inode root_inode;
        sfs_read_inode(ROOT_INO, &root_inode);
        for (u32 bi = 0; bi < root_inode.block_count; bi++) {
            block_read(root_inode.blocks[bi], block);
            u32 dentries_per_block = BLOCK_SIZE / sizeof(struct dentry);
            for (u32 di = 0; di < dentries_per_block; di++) {
                u32 off = di * sizeof(struct dentry);
                struct dentry d;
                for (u32 i = 0; i < sizeof(struct dentry); i++)
                    ((u8 *)&d)[i] = block[off + i];
                if (d.ino != 0)
                    serial_printf(SERIAL_COM1, "  %s (ino=%d)\n", d.name, d.ino);
            }
        }
    }
    serial_printf(SERIAL_COM1, "== end ==\n\n");
}
