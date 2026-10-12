/* Bounded JSON reader for Discord RPC replies. No allocations or game access. */
#include "presence_json.h"
#include "support.h"

#define JSON_MAX_DEPTH 32u
typedef struct {
    const char *text;
    size_t size, at;
    const char *parent, *key;
    char *out;
    size_t capacity;
    int seen_parent, seen_field, found;
} JsonReader;

static void whitespace(JsonReader *r) {
    while (r->at < r->size) {
        char c = r->text[r->at];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        ++r->at;
    }
}

static int hex4(JsonReader *r, unsigned *value) {
    *value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        if (r->at == r->size) return 0;
        unsigned char c = (unsigned char)r->text[r->at++];
        unsigned n;
        if (c >= '0' && c <= '9') n = c - '0';
        else if (c >= 'a' && c <= 'f') n = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') n = c - 'A' + 10;
        else return 0;
        *value = (*value << 4) | n;
    }
    return 1;
}

static void string_byte(char *out, size_t capacity, size_t *used, unsigned char c) {
    if (out && *used + 1 < capacity) out[*used] = (char)c;
    ++*used;
}

static int string(JsonReader *r, char *out, size_t capacity, size_t *length) {
    size_t used = 0;
    if (r->at == r->size || r->text[r->at++] != '"') return 0;
    while (r->at < r->size) {
        unsigned char c = (unsigned char)r->text[r->at++];
        if (c == '"') {
            if (out && capacity) out[used < capacity ? used : capacity - 1] = 0;
            *length = used;
            return 1;
        }
        if (c < 32) return 0;
        if (c == '\\') {
            if (r->at == r->size) return 0;
            c = (unsigned char)r->text[r->at++];
            switch (c) {
                case '"': case '\\': case '/': break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'u': {
                    unsigned code;
                    if (!hex4(r, &code)) return 0;
                    if (code >= 0xd800 && code <= 0xdbff) {
                        unsigned low;
                        if (r->at + 2 > r->size || r->text[r->at++] != '\\' ||
                            r->text[r->at++] != 'u' || !hex4(r, &low) ||
                            low < 0xdc00 || low > 0xdfff) return 0;
                        code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
                    } else if (code >= 0xdc00 && code <= 0xdfff) return 0;
                    /* C strings cannot represent embedded NUL unambiguously. */
                    if (!code) return 0;
                    if (code < 0x80) c = (unsigned char)code;
                    else {
                        if (code >= 0x10000) string_byte(out, capacity, &used, (unsigned char)(0xf0 | (code >> 18)));
                        if (code >= 0x800) string_byte(out, capacity, &used,
                            (unsigned char)((code < 0x10000 ? 0xe0 : 0x80) | ((code >> 12) & 0x3f)));
                        string_byte(out, capacity, &used,
                            (unsigned char)((code < 0x800 ? 0xc0 : 0x80) | ((code >> 6) & 0x3f)));
                        c = (unsigned char)(0x80 | (code & 0x3f));
                    }
                    break;
                }
                default: return 0;
            }
        } else if (c >= 0x80) {
            /* Validate literal UTF-8, including overlongs and surrogate ranges. */
            unsigned count, code, minimum;
            if (c >= 0xc2 && c <= 0xdf) { count = 1; code = c & 31; minimum = 0x80; }
            else if (c >= 0xe0 && c <= 0xef) { count = 2; code = c & 15; minimum = 0x800; }
            else if (c >= 0xf0 && c <= 0xf4) { count = 3; code = c & 7; minimum = 0x10000; }
            else return 0;
            string_byte(out, capacity, &used, c);
            for (unsigned i = 0; i < count; ++i) {
                if (r->at == r->size) return 0;
                c = (unsigned char)r->text[r->at++];
                if ((c & 0xc0) != 0x80) return 0;
                code = (code << 6) | (c & 63);
                string_byte(out, capacity, &used, c);
            }
            if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return 0;
            continue;
        }
        string_byte(out, capacity, &used, c);
    }
    return 0;
}

static int literal(JsonReader *r, const char *s) {
    size_t size = text_length(s);
    if (size > r->size - r->at || !memory_equal(r->text + r->at, s, size)) return 0;
    r->at += size;
    return 1;
}

static int digit(JsonReader *r) {
    return r->at < r->size && r->text[r->at] >= '0' && r->text[r->at] <= '9';
}

static int value(JsonReader *r, unsigned depth, int scope);
static int object(JsonReader *r, unsigned depth, int scope) {
    ++r->at;
    whitespace(r);
    if (r->at < r->size && r->text[r->at] == '}') { ++r->at; return 1; }
    for (;;) {
        char key[64]; size_t length;
        if (!string(r, key, sizeof(key), &length)) return 0;
        whitespace(r);
        if (r->at == r->size || r->text[r->at++] != ':') return 0;
        whitespace(r);
        if (scope == 1 && r->parent && length == text_length(r->parent) &&
            length < sizeof(key) && memory_equal(key, r->parent, length)) {
            if (r->seen_parent++) return 0;
            if (r->at == r->size || r->text[r->at] != '{' || !value(r, depth + 1, 2)) return 0;
        } else if (((scope == 1 && !r->parent) || scope == 2) &&
                   length == text_length(r->key) && length < sizeof(key) &&
                   memory_equal(key, r->key, length)) {
            if (r->seen_field++) return 0;
            if (r->at < r->size && r->text[r->at] == '"') {
                if (!string(r, r->out, r->capacity, &length) || length >= r->capacity) return 0;
                r->found = 1;
            } else if (!literal(r, "null")) return 0;
        } else if (!value(r, depth + 1, 0)) return 0;
        whitespace(r);
        if (r->at == r->size) return 0;
        char c = r->text[r->at++];
        if (c == '}') return 1;
        if (c != ',') return 0;
        whitespace(r);
    }
}

static int value(JsonReader *r, unsigned depth, int scope) {
    whitespace(r);
    if (depth > JSON_MAX_DEPTH || r->at == r->size) return 0;
    char c = r->text[r->at];
    if (c == '{') return object(r, depth, scope);
    if (c == '[') {
        ++r->at; whitespace(r);
        if (r->at < r->size && r->text[r->at] == ']') { ++r->at; return 1; }
        for (;;) {
            if (!value(r, depth + 1, 0)) return 0;
            whitespace(r);
            if (r->at == r->size) return 0;
            c = r->text[r->at++];
            if (c == ']') return 1;
            if (c != ',') return 0;
        }
    }
    if (c == '"') { size_t length; return string(r, 0, 0, &length); }
    if (c == 't') return literal(r, "true");
    if (c == 'f') return literal(r, "false");
    if (c == 'n') return literal(r, "null");
    if (c == '-') ++r->at;
    if (!digit(r)) return 0;
    if (r->text[r->at] == '0') ++r->at;
    else while (digit(r)) ++r->at;
    if (r->at < r->size && r->text[r->at] == '.') {
        ++r->at;
        if (!digit(r)) return 0;
        while (digit(r)) ++r->at;
    }
    if (r->at < r->size && (r->text[r->at] == 'e' || r->text[r->at] == 'E')) {
        ++r->at;
        if (r->at < r->size && (r->text[r->at] == '+' || r->text[r->at] == '-')) ++r->at;
        if (!digit(r)) return 0;
        while (digit(r)) ++r->at;
    }
    return 1;
}

int presence_json_field(const char *json, size_t size, const char *parent,
                        const char *key, char *out, size_t capacity) {
    if (!json || !key || !out || !capacity) return -1;
    out[0] = 0;
    JsonReader r = {.text = json, .size = size, .parent = parent, .key = key,
                    .out = out, .capacity = capacity};
    whitespace(&r);
    if (r.at == size || json[r.at] != '{' || !value(&r, 0, 1)) { out[0] = 0; return -1; }
    whitespace(&r);
    if (r.at != size) { out[0] = 0; return -1; }
    return r.found;
}
