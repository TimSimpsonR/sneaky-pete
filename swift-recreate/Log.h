#ifndef FILE_LOG_H
#define FILE_LOG_H

#include <stdio.h>

#define NOVA_LOG_DEBUG(fmt, ...) printf("%s, line %d:" fmt, __FILE__, __LINE__, ##__VA_ARGS__);
#define NOVA_LOG_INFO(fmt, ...) printf("%s, line %d:" fmt, __FILE__, __LINE__, ##__VA_ARGS__);
#define NOVA_LOG_ERROR(fmt, ...) printf("%s, line %d:" fmt, __FILE__, __LINE__, ##__VA_ARGS__);

#endif
