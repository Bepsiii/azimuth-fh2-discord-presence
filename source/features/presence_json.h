#ifndef FH2_PRESENCE_JSON_H
#define FH2_PRESENCE_JSON_H
#include <stddef.h>

/* Read a string in the root object, or in one named child object. The entire
   document is checked; nested lookalikes and duplicate requested fields fail.
   Returns 1 for a string, 0 for missing/null, -1 for invalid input/overflow. */
int presence_json_field(const char *json, size_t size, const char *parent,
                        const char *key, char *out, size_t capacity);
#endif
