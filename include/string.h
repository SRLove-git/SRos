#ifndef STRING_H 
#define STRING_H 
#include "types.h"



void memset(void *str, int c, u32 n);
u32 strlen(const char *str);
int strcmp(const char *str1, const char *str2);
int strncmp(const char* str1,const char* str2,u32 num);
char *strcpy(char *dest, const char *src);
void *memcpy(void *dest, const void *src, u32 n);


#endif /* STRING_H */