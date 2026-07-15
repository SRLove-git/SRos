#ifndef SHELL_H
#define SHELL_H

void shell_main(void);
int getchar(void);
void putchar(char c);
void execute(char *cmd);
int parse_command(char *line, char *argv[], int max_args);
void cmd_echo(int argc, char *argv[]);
void cmd_clear(int argc, char *argv[]);
void puts(char *str);

/* 文件系统包装函数（用户态） */
int fs_open(const char *path);
int fs_read(int fd, char *buf, int count);
int fs_write(int fd, const char *buf, int count);
void fs_close(int fd);
int fs_create(const char *name);
void fs_ls(void);

#endif /* SHELL_H */
