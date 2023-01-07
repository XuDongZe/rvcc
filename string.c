#include "rvcc.h"

// 将字符串格式化
char *format(char *fmt, ...)
{
    char *buf;
    size_t len;

    // 将字符串对应的内存作为IO流
    FILE *out = open_memstream(&buf, &len);

    va_list va;
    va_start(va, fmt);
    // 向流中写入数据
    vfprintf(out, fmt, va);
    va_end(va);

    fclose(out);
    return buf;
}