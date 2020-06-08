#include "md5.h"

void transform_to_md5(const unsigned char *in_buf, size_t in_len, unsigned char *md)
{
    MD5(in_buf, in_len, md);
}

