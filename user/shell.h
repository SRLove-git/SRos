#ifndef SHELL_H
#define SHELL_H

void shell_main(void);
char getchar(void);
void putchar(char c);
void execute(char *cmd);
int parse_command(char *line, char *argv[], int max_args);
void cmd_echo(int argc, char *argv[]);
void cmd_clear(int argc, char *argv[]);
void puts(char *str);

#endif /* SHELL_H */