# Public C API
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "zh_config.h"
#include "zh_types.h"
#include "zh_stats.h"

#ifdef __cplusplus
extern "C" {
#endif

void  zh_init(void);
void  zh_shutdown(void);
void  zh_thread_shutdown(void);

void* zh_malloc(size_t size);
void  zh_free(void* ptr);
void* zh_realloc(void* ptr, size_t size);
size_t zh_usable_size(void* ptr);

#ifdef __cplusplus
}
#endif
