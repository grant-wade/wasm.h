/* Usage: wasm_amalgamate_test <amalgamate executable> <scratch directory> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#include <sys/utime.h>
#define MAKE_DIR(path) _mkdir(path)
#else
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>
#define MAKE_DIR(path) mkdir((path), 0755)
#endif

static const char* tool;
static const char* root;
static int failures;

#define CHECK(condition, message)                                   \
    do {                                                            \
        if (!(condition)) {                                         \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, message); \
            failures++;                                             \
        }                                                           \
    } while (0)

static char* path_of(const char* relative) {
    char* path = (char*)malloc(strlen(root) + strlen(relative) + 2);
    if (!path) exit(1);
    sprintf(path, "%s/%s", root, relative);
    return path;
}

static void write_text(const char* relative, const char* text, const char* mode) {
    char* path = path_of(relative);
    FILE* f = fopen(path, mode);
    if (!f) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    fputs(text, f);
    fclose(f);
    free(path);
}

static char* read_text(const char* relative) {
    char* path = path_of(relative);
    FILE* f = fopen(path, "rb");
    free(path);
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* text = (char*)malloc((size_t)size + 1);
    if (!text) exit(1);
    size_t n = fread(text, 1, (size_t)size, f);
    text[n] = '\0';
    fclose(f);
    return text;
}

static long modified_time(const char* relative) {
    char* path = path_of(relative);
    struct stat st;
    long result = stat(path, &st) == 0 ? (long)st.st_mtime : -1;
    free(path);
    return result;
}

/* An unwanted rewrite then visibly changes the timestamp. */
static void backdate(const char* relative) {
    char* path = path_of(relative);
#if defined(_WIN32)
    struct _utimbuf times;
    times.actime = times.modtime = 1000000000;
    _utime(path, &times);
#else
    struct utimbuf times;
    times.actime = times.modtime = 1000000000;
    utime(path, &times);
#endif
    free(path);
}

static int run_tool(int check) {
    char* stamp = path_of("header.stamp");
    const char* argv[7];
    int argc = 0;
    argv[argc++] = tool;
    argv[argc++] = "--root";
    argv[argc++] = root;
    argv[argc++] = "--stamp";
    argv[argc++] = stamp;
    if (check) argv[argc++] = "--check";
    argv[argc] = NULL;
    int status;
#if defined(_WIN32)
    status = (int)_spawnv(_P_WAIT, tool, argv);
#else
    pid_t pid = fork();
    if (pid == 0) {
        execv(tool, (char* const*)argv);
        _exit(127);
    }
    status = -1;
    if (pid > 0 && waitpid(pid, &status, 0) == pid) status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    free(stamp);
    return status;
}

static int count_occurrences(const char* text, const char* needle) {
    int count = 0;
    for (const char* p = strstr(text, needle); p; p = strstr(p + 1, needle)) count++;
    return count;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <amalgamate> <scratch directory>\n", argv[0]);
        return 2;
    }
    tool = argv[1];
    root = argv[2];

    MAKE_DIR(root);
    char* src = path_of("src");
    char* nested = path_of("src/nested");
    MAKE_DIR(src);
    MAKE_DIR(nested);
    char* header = path_of("wasm.h");
    remove(header);
    free(src);
    free(nested);
    free(header);
    write_text("src/amalgamation.h",
               "#include <stdint.h>\n#include \"nested/one.h\"\n#include \"nested/one.h\"\n#include \"runtime.c\"\nint tail;\n",
               "wb");
    write_text("src/runtime.c", "#include \"nested/one.h\"\nint runtime;\n", "wb");
    write_text("src/nested/one.h", "int first;\n#include \"two.inc\"\n", "wb");
    write_text("src/nested/two.inc", "#define VALUES a; b; c\n#define CONTINUED \\\n  42\n", "wb");

    CHECK(run_tool(0) == 0, "Unexpected generator failure");
    char* generated = read_text("wasm.h");
    char* fingerprint = read_text("header.stamp");
    CHECK(generated && fingerprint && *fingerprint, "Generator wrote no header or stamp");
    if (!generated || !fingerprint) {
        free(generated);
        free(fingerprint);
        return 1;
    }

    static const char* const kept[] = {
        "#include <stdint.h>", "int first;", "int runtime;", "#define VALUES a; b; c", "#define CONTINUED \\\n  42",
        "int tail;",
    };
    for (size_t i = 0; i < sizeof(kept) / sizeof(kept[0]); ++i) {
        if (!strstr(generated, kept[i])) {
            fprintf(stderr, "Lost source text: %s\n", kept[i]);
            failures++;
        }
    }
    CHECK(count_occurrences(generated, "BEGIN src/nested/one.h") == 1 && !strstr(generated, "#include \""),
          "Recursive include expansion or deduplication failed");

    backdate("wasm.h");
    long before = modified_time("wasm.h");
    CHECK(run_tool(0) == 0, "Unexpected generator failure");
    CHECK(modified_time("wasm.h") == before, "Unchanged output was rewritten");
    char* same_fingerprint = read_text("header.stamp");
    CHECK(same_fingerprint && strcmp(same_fingerprint, fingerprint) == 0, "Build stamp does not identify generated content");

    CHECK(run_tool(1) == 0, "Check mode rejected a fresh header");
    write_text("src/nested/two.inc", "int changed;\n", "ab");
    CHECK(run_tool(1) != 0, "Check mode accepted a stale header");
    char* checked = read_text("wasm.h");
    CHECK(checked && strcmp(checked, generated) == 0, "Check mode changed output");
    char* checked_fingerprint = read_text("header.stamp");
    CHECK(checked_fingerprint && strcmp(checked_fingerprint, fingerprint) == 0, "Check mode changed the build stamp");

    CHECK(run_tool(0) == 0, "Unexpected generator failure");
    char* changed_fingerprint = read_text("header.stamp");
    CHECK(changed_fingerprint && strcmp(changed_fingerprint, fingerprint) != 0,
          "Changed content did not update the build stamp");

    write_text("src/nested/two.inc", "#include \"../amalgamation.h\"\n", "wb");
    CHECK(run_tool(0) != 0, "Recursive include was accepted");
    write_text("src/nested/two.inc", "#include \"../../outside.h\"\n", "wb");
    write_text("outside.h", "int outside;\n", "wb");
    CHECK(run_tool(0) != 0, "Include outside src/ was accepted");
    char* two = path_of("src/nested/two.inc");
    remove(two);
    free(two);
    CHECK(run_tool(0) != 0, "Missing include was accepted");

    free(generated);
    free(fingerprint);
    free(same_fingerprint);
    free(checked);
    free(checked_fingerprint);
    free(changed_fingerprint);
    if (failures) fprintf(stderr, "%d amalgamation check(s) failed\n", failures);
    return failures ? 1 : 0;
}
