#include "attr.h"

// this file is standalone at top of memory

static unsigned char stupid_data[0x100];

__attribute_used__ int some_func() {
    return 10 + stupid_data[0x5];
}

__attribute_used__ void set_stupid_data(int offset, unsigned char value) {
    stupid_data[offset] = value;
}
