/* Minimal freestanding memory primitives; no Microsoft C runtime required. */
#include <stddef.h>
void *memset(void *destination, int value, size_t count) {
    volatile unsigned char *out = destination;
    for (size_t i = 0; i < count; ++i) out[i] = (unsigned char)value;
    return destination;
}
void *memcpy(void *destination, const void *source, size_t count) {
    volatile unsigned char *out = destination;
    const volatile unsigned char *in = source;
    for (size_t i = 0; i < count; ++i) out[i] = in[i];
    return destination;
}
void *memmove(void *destination, const void *source, size_t count) {
    volatile unsigned char *out = destination;
    const volatile unsigned char *in = source;
    if ((size_t)out > (size_t)in) { while (count) { --count; out[count] = in[count]; } }
    else { for (size_t i = 0; i < count; ++i) out[i] = in[i]; }
    return destination;
}
