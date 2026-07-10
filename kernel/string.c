#include "string.h"
//strlen 、 strcmp 、 strncmp 、 strcpy 、 memset


u32 strlen(const char *str){
    u32 len = 0;
    while(str[len] != '\0'){
        len++;
    }
    return len;
}
int strcmp(const char *str1, const char *str2){
    while((*str1) && (*str1 == *str2)){
        str1++;
        str2++;
    }
    if(*(u8*)str1 > *(u8*)str2){
        return 1;
    }else if(*(u8*)str1 < *(u8*)str2){
        return -1;
    }else{
        return 0;
    }
}

int strncmp(const char* str1,const char* str2,u32 num){
    if(num == 0){
        return 0;
    }
    u32 i = 0;
    while(i < num){
        u8 c1 = str1[i];
        u8 c2 = str2[i];
        if(c1!=c2){
            return (int)(c1-c2);
        }
        if(c1 == '\0'){
            return 0;
        }
        i++;
    }
    return 0;
}
char *strcpy(char *dest, const char *src){
    char *ret = dest;       // 保存目标字符串起始地址
    while ((*dest++ = *src++) != '\0'); // 逐字节复制，直到结束符
    return ret; 
}

void memset(void *str, int c, u32 n){
    unsigned char *s = (unsigned char *)str;
    while(n--){
        s[n] = c;
    }
}
