/* Usage: amalgamate [--check] [--root DIR] [--stamp FILE]
 *
 *   --check   exit 1 if wasm.h is stale instead of rewriting it
 *   --root    repository root (default: the current directory)
 *   --stamp   write a fingerprint of the generated text to FILE on every run
 */
#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AMALGAMATE_HEADER                                         \
    "/* Generated from src/amalgamation.h. Do not edit directly.\n" \
    " * Regenerate: ./cbuild -t wasm-header\n"                     \
    " */\n"

typedef struct {
    char* data;
    size_t len;
    size_t cap;
} buffer_t;

typedef struct {
    char** items;
    int count;
    int cap;
} list_t;

static const char* root = ".";
static list_t included;

static void fail(const char* format, ...) {
    va_list args;
    fputs("amalgamate: ", stderr);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    exit(1);
}

static void* xrealloc(void* p, size_t size) {
    void* q = realloc(p, size ? size : 1);
    if (!q) fail("out of memory");
    return q;
}

static void append(buffer_t* b, const char* data, size_t len) {
    if (b->len + len + 1 > b->cap) {
        while (b->len + len + 1 > b->cap) b->cap = b->cap ? b->cap * 2 : 4096;
        b->data = (char*)xrealloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, data, len);
    b->len += len;
    b->data[b->len] = '\0';
}

static void append_str(buffer_t* b, const char* s) {
    append(b, s, strlen(s));
}

static void list_push(list_t* l, const char* s) {
    if (l->count == l->cap) {
        l->cap = l->cap ? l->cap * 2 : 64;
        l->items = (char**)xrealloc(l->items, sizeof(char*) * (size_t)l->cap);
    }
    l->items[l->count] = (char*)xrealloc(NULL, strlen(s) + 1);
    strcpy(l->items[l->count], s);
    l->count++;
}

static int list_has(const list_t* l, const char* s) {
    for (int i = 0; i < l->count; ++i) {
        if (strcmp(l->items[i], s) == 0) return 1;
    }
    return 0;
}

static char* join_root(const char* relative) {
    char* path = (char*)xrealloc(NULL, strlen(root) + strlen(relative) + 2);
    sprintf(path, "%s/%s", root, relative);
    return path;
}

static int read_file(const char* path, buffer_t* out) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    char chunk[65536];
    size_t n;
    out->len = 0;
    append(out, "", 0);
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) append(out, chunk, n);
    fclose(f);
    return 1;
}

static void write_file(const char* path, const char* data, size_t len) {
    FILE* f = fopen(path, "wb");
    if (!f || fwrite(data, 1, len, f) != len || fclose(f) != 0) fail("cannot write %s", path);
}

/* Resolves include against the directory of the root-relative path from,
 * lexically. A result outside the root keeps its leading "../" segments. */
static char* resolve(const char* from, const char* include) {
    size_t dir_len = 0;
    const char* slash = strrchr(from, '/');
    if (slash) dir_len = (size_t)(slash - from);
    char* joined = (char*)xrealloc(NULL, dir_len + strlen(include) + 2);
    memcpy(joined, from, dir_len);
    joined[dir_len] = '\0';
    if (dir_len) strcat(joined, "/");
    strcat(joined, include);

    char* out = (char*)xrealloc(NULL, strlen(joined) + 1);
    size_t out_len = 0;
    int ups = 0; /* leading ".." segments that could not be cancelled */
    for (char* seg = joined; *seg;) {
        char* end = seg + strcspn(seg, "/\\");
        size_t seg_len = (size_t)(end - seg);
        if (seg_len == 0 || (seg_len == 1 && seg[0] == '.')) {
            /* nothing */
        } else if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') {
            if (out_len > (size_t)ups * 3) {
                while (out_len > 0 && out[out_len - 1] != '/') out_len--;
                if (out_len > 0) out_len--;
            } else {
                if (out_len) out[out_len++] = '/';
                memcpy(out + out_len, "..", 2);
                out_len += 2;
                ups++;
            }
        } else {
            if (out_len) out[out_len++] = '/';
            memcpy(out + out_len, seg, seg_len);
            out_len += seg_len;
        }
        seg = *end ? end + 1 : end;
    }
    out[out_len] = '\0';
    free(joined);
    return out;
}

static int is_blank(char c) {
    return c == ' ' || c == '\t';
}

/* Matches [ \t]*#[ \t]*include[ \t]+"([^"\n]+)"[^\n]*(\n|$) at s[i]. On a
 * match, stores the include name and the end of the directive. */
static int match_directive(const char* s, size_t len, size_t i, size_t* name, size_t* name_len, size_t* end) {
    while (i < len && is_blank(s[i])) i++;
    if (i >= len || s[i] != '#') return 0;
    i++;
    while (i < len && is_blank(s[i])) i++;
    if (len - i < 7 || memcmp(s + i, "include", 7) != 0) return 0;
    i += 7;
    if (i >= len || !is_blank(s[i])) return 0;
    while (i < len && is_blank(s[i])) i++;
    if (i >= len || s[i] != '"') return 0;
    size_t start = ++i;
    while (i < len && s[i] != '"' && s[i] != '\n') i++;
    if (i >= len || s[i] != '"' || i == start) return 0;
    *name = start;
    *name_len = i - start;
    i++;
    while (i < len && s[i] != '\n') i++;
    if (i < len) i++;
    *end = i;
    return 1;
}

/* Leftmost (^|\n)<directive>; *start is the newline that belongs to the match, if any. */
static int find_directive(const char* s, size_t len, size_t* start, size_t* name, size_t* name_len,
                          size_t* end) {
    for (size_t p = 0; p < len; ++p) {
        if (p == 0 && match_directive(s, len, 0, name, name_len, end)) {
            *start = 0;
            return 1;
        }
        if (s[p] == '\n' && match_directive(s, len, p + 1, name, name_len, end)) {
            *start = p;
            return 1;
        }
    }
    return 0;
}

static int has_suffix(const char* s, const char* suffix) {
    size_t n = strlen(s), m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

static void expand(const char* label, list_t* stack, buffer_t* out) {
    if (list_has(stack, label)) fail("Recursive local include: %s", label);
    if (list_has(&included, label)) return;

    buffer_t text = { 0 };
    char* path = join_root(label);
    if (!read_file(path, &text)) fail("Missing local include: %s", label);
    free(path);
    if (strncmp(label, "src/", 4) != 0 || strncmp(label, "../", 3) == 0 ||
        !(has_suffix(label, ".h") || has_suffix(label, ".inc") || has_suffix(label, ".c"))) {
        fail("Local include must be a .h, .inc, or .c file under src/: %s", label);
    }
    list_push(&included, label);
    list_push(stack, label);

    size_t begin = out->len;
    append_str(out, "/* BEGIN ");
    append_str(out, label);
    append_str(out, " */\n");

    /* Byte-exact: an include line becomes a newline plus the expansion. */
    const char* remaining = text.data;
    size_t remaining_len = text.len;
    size_t start, name, name_len, end;
    while (find_directive(remaining, remaining_len, &start, &name, &name_len, &end)) {
        append(out, remaining, start);
        append(out, "\n", 1);
        char* include = (char*)xrealloc(NULL, name_len + 1);
        memcpy(include, remaining + name, name_len);
        include[name_len] = '\0';
        char* child = resolve(label, include);
        expand(child, stack, out);
        free(child);
        free(include);
        remaining += end;
        remaining_len -= end;
    }
    append(out, remaining, remaining_len);
    if (out->len == begin || out->data[out->len - 1] != '\n') append(out, "\n", 1);
    append_str(out, "/* END ");
    append_str(out, label);
    append_str(out, " */\n");

    free(stack->items[--stack->count]);
    free(text.data);
}

static unsigned long long fingerprint(const char* data, size_t len) {
    unsigned long long h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= (unsigned char)data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

int main(int argc, char** argv) {
    int check = 0;
    const char* stamp = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--check") == 0) {
            check = 1;
        } else if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = argv[++i];
        } else if (strcmp(argv[i], "--stamp") == 0 && i + 1 < argc) {
            stamp = argv[++i];
        } else {
            fprintf(stderr, "usage: %s [--check] [--root DIR] [--stamp FILE]\n", argv[0]);
            return 2;
        }
    }

    buffer_t generated = { 0 };
    list_t stack = { 0 };
    append_str(&generated, AMALGAMATE_HEADER);
    expand("src/amalgamation.h", &stack, &generated);

    char* destination = join_root("wasm.h");
    buffer_t existing = { 0 };
    int have_existing = read_file(destination, &existing);
    int same = have_existing && existing.len == generated.len &&
               memcmp(existing.data, generated.data, generated.len) == 0;
    if (!same) {
        if (check) {
            fprintf(stderr, "amalgamate: wasm.h is stale. Run: ./cbuild -t wasm-header\n");
            return 1;
        }
        write_file(destination, generated.data, generated.len);
        printf("amalgamate: wrote wasm.h\n");
    }

    if (stamp && !check) {
        char text[32];
        snprintf(text, sizeof(text), "%016llx\n", fingerprint(generated.data, generated.len));
        write_file(stamp, text, strlen(text));
    }
    return 0;
}
