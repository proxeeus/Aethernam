/* Segment 0f23: file size helper. */
#include "../core/mem.h"
#include "../platform/platform.h"
#include "protos.h"

/* 0f23:000a  returns the size of file `name` (DOS open/lseek-end/close), -1 if it cannot be opened */
int32_t sub_0f23_000a(uint8_t *name)
{
    return plat_file_size((const char *)name);
}
