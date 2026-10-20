#include "../support.h"

#ifdef WASM_TEST_MODULAR
#include "../../../src/portable.h"
#endif

#if defined(_WIN32)
#include <io.h>
#define WASM_TEST_DUP _dup
#define WASM_TEST_DUP2 _dup2
#define WASM_TEST_CLOSE _close
#define WASM_TEST_FILENO _fileno
#else
#include <unistd.h>
#define WASM_TEST_DUP dup
#define WASM_TEST_DUP2 dup2
#define WASM_TEST_CLOSE close
#define WASM_TEST_FILENO fileno
#endif

#if WASM_ENABLE_PLATFORM
typedef struct wasm_test_stream_capture {
    FILE* stream;
    FILE* temp;
    int saved_fd;
    int stream_fd;
} wasm_test_stream_capture;

static int wasm_test_begin_stream_capture(wasm_test_stream_capture* capture, FILE* stream) {
    int temp_fd;

    if (!capture || !stream) return 0;

    memset(capture, 0, sizeof(*capture));
    capture->stream = stream;
    capture->stream_fd = WASM_TEST_FILENO(stream);
    if (capture->stream_fd < 0) return 0;

    fflush(stream);
    capture->saved_fd = WASM_TEST_DUP(capture->stream_fd);
    if (capture->saved_fd < 0) return 0;

    capture->temp = tmpfile();
    if (!capture->temp) {
        WASM_TEST_CLOSE(capture->saved_fd);
        capture->saved_fd = -1;
        return 0;
    }

    temp_fd = WASM_TEST_FILENO(capture->temp);
    if (temp_fd < 0 || WASM_TEST_DUP2(temp_fd, capture->stream_fd) < 0) {
        fclose(capture->temp);
        capture->temp = NULL;
        WASM_TEST_CLOSE(capture->saved_fd);
        capture->saved_fd = -1;
        return 0;
    }

    return 1;
}

static size_t wasm_test_end_stream_capture(wasm_test_stream_capture* capture,
                                           char* buffer,
                                           size_t buffer_size) {
    size_t bytes_read = 0;

    if (!capture || !capture->temp) return 0;

    fflush(capture->stream);
    fflush(capture->temp);
    if (buffer && buffer_size > 0) {
        long end_pos;

        if (fseek(capture->temp, 0, SEEK_END) == 0) {
            end_pos = ftell(capture->temp);
            if (end_pos > 0) {
                size_t max_read = buffer_size - 1u;

                if ((size_t)end_pos < max_read) max_read = (size_t)end_pos;
                if (fseek(capture->temp, 0, SEEK_SET) == 0)
                    bytes_read = fread(buffer, 1u, max_read, capture->temp);
            }
        }
        buffer[bytes_read] = '\0';
    }

    WASM_TEST_DUP2(capture->saved_fd, capture->stream_fd);
    WASM_TEST_CLOSE(capture->saved_fd);
    fclose(capture->temp);
    memset(capture, 0, sizeof(*capture));
    return bytes_read;
}

TEST(test_public_wasi_stub_helpers) {
    static const uint8_t wasi_message[] = "hello wasi\n";
    static const uint8_t wasi_iov[] = {
        64u,
        0u,
        0u,
        0u,
        (uint8_t)sizeof(wasi_message) - 1u,
        0u,
        0u,
        0u,
    };
    static const uint8_t wasi_seek_initial[8] = {
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
        0xFFu,
    };

    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_error_t err;
    wasm_value_t result;
    wasm_test_stream_capture capture = { 0 };
    uint8_t nwritten_bytes[4];
    uint8_t seek_offset_bytes[8];
    uint32_t nwritten = 0;
    uint64_t seek_offset = UINT64_MAX;
    char output[64];
    size_t output_len;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 7);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 2);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 0);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 4);
        emit(&sec, 0x7F);
        emit(&sec, 0x7E);
        emit(&sec, 0x7F);
        emit(&sec, 0x7F);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);

        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 0);

        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 5);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_write", 0);
        emit_import_func(&sec, "wasi_snapshot_preview1", "environ_get", 1);
        emit_import_func(&sec, "wasi_snapshot_preview1", "proc_exit", 2);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_close", 3);
        emit_import_func(&sec, "wasi_snapshot_preview1", "fd_seek", 4);
        emit_section(&mod, 2, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 5);
        emit_leb128_u32(&sec, 5);
        emit_leb128_u32(&sec, 5);
        emit_leb128_u32(&sec, 5);
        emit_leb128_u32(&sec, 5);
        emit_leb128_u32(&sec, 6);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x00);
        emit_leb128_u32(&sec, 1);
        emit_section(&mod, 5, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 5);
        emit_export_func(&sec, "run", 5);
        emit_export_func(&sec, "env", 6);
        emit_export_func(&sec, "close", 7);
        emit_export_func(&sec, "seek", 8);
        emit_export_func(&sec, "quit", 9);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t run_body = { 0 };
        wasm_builder_t env_body = { 0 };
        wasm_builder_t close_body = { 0 };
        wasm_builder_t seek_body = { 0 };
        wasm_builder_t quit_body = { 0 };

        emit_leb128_u32(&sec, 5);

        emit_leb128_u32(&run_body, 0);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 1);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 32);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 1);
        emit(&run_body, 0x41);
        emit_leb128_i32(&run_body, 48);
        emit(&run_body, 0x10);
        emit_leb128_u32(&run_body, 0);
        emit(&run_body, 0x0B);
        emit_leb128_u32(&sec, run_body.len);
        emit_bytes(&sec, run_body.buf, run_body.len);

        emit_leb128_u32(&env_body, 0);
        emit(&env_body, 0x41);
        emit_leb128_i32(&env_body, 0);
        emit(&env_body, 0x41);
        emit_leb128_i32(&env_body, 0);
        emit(&env_body, 0x10);
        emit_leb128_u32(&env_body, 1);
        emit(&env_body, 0x0B);
        emit_leb128_u32(&sec, env_body.len);
        emit_bytes(&sec, env_body.buf, env_body.len);

        emit_leb128_u32(&close_body, 0);
        emit(&close_body, 0x41);
        emit_leb128_i32(&close_body, 1);
        emit(&close_body, 0x10);
        emit_leb128_u32(&close_body, 3);
        emit(&close_body, 0x0B);
        emit_leb128_u32(&sec, close_body.len);
        emit_bytes(&sec, close_body.buf, close_body.len);

        emit_leb128_u32(&seek_body, 0);
        emit(&seek_body, 0x41);
        emit_leb128_i32(&seek_body, 1);
        emit(&seek_body, 0x42);
        emit_leb128_i64(&seek_body, 0);
        emit(&seek_body, 0x41);
        emit_leb128_i32(&seek_body, 1);
        emit(&seek_body, 0x41);
        emit_leb128_i32(&seek_body, 80);
        emit(&seek_body, 0x10);
        emit_leb128_u32(&seek_body, 4);
        emit(&seek_body, 0x0B);
        emit_leb128_u32(&sec, seek_body.len);
        emit_bytes(&sec, seek_body.buf, seek_body.len);

        emit_leb128_u32(&quit_body, 0);
        emit(&quit_body, 0x41);
        emit_leb128_i32(&quit_body, 5);
        emit(&quit_body, 0x10);
        emit_leb128_u32(&quit_body, 2);
        emit(&quit_body, 0x00);
        emit(&quit_body, 0x0B);
        emit_leb128_u32(&sec, quit_body.len);
        emit_bytes(&sec, quit_body.buf, quit_body.len);

        emit_section(&mod, 10, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };

        emit_leb128_u32(&sec, 3);

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 32);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(wasi_iov));
        emit_bytes(&sec, wasi_iov, (uint32_t)sizeof(wasi_iov));

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 64);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(wasi_message) - 1u);
        emit_bytes(&sec, wasi_message, (uint32_t)sizeof(wasi_message) - 1u);

        emit_leb128_u32(&sec, 0);
        emit(&sec, 0x41);
        emit_leb128_i32(&sec, 80);
        emit(&sec, 0x0B);
        emit_leb128_u32(&sec, (uint32_t)sizeof(wasi_seek_initial));
        emit_bytes(&sec, wasi_seek_initial, (uint32_t)sizeof(wasi_seek_initial));

        emit_section(&mod, 11, sec.buf, sec.len);
    }

    wasm_init(&rt);
    err = wasm_bind_wasi_stubs(&rt);
    WASM_CHECK_OK(t, err);

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m != NULL, "%s", rt.error_msg);
    if (m == NULL) {
        wasm_destroy(&rt);
        return;
    }

    CHECK_MSG(t, wasm_test_begin_stream_capture(&capture, stdout), "%s", "failed to capture stdout");
    err = wasm_call(m, "run", NULL, 0, &result, 1);
    output_len = wasm_test_end_stream_capture(&capture, output, sizeof(output));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);
    CHECK_MSG(t, output_len == sizeof(wasi_message) - 1u,
              "expected %u bytes of wasi output, got %zu",
              (unsigned)(sizeof(wasi_message) - 1u), output_len);
    CHECK_MSG(t, strcmp(output, (const char*)wasi_message) == 0,
              "expected captured wasi output '%s', got '%s'",
              (const char*)wasi_message,
              output);

    err = wasm_memory_read(m, 0, 48, nwritten_bytes, sizeof(nwritten_bytes));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        nwritten = wasm__load_le32(nwritten_bytes);
        WASM_CHECK_I32(t, (int32_t)nwritten, (int32_t)(sizeof(wasi_message) - 1u));
    }

    err = wasm_call(m, "env", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);

    err = wasm_call(m, "close", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);

    err = wasm_call(m, "seek", NULL, 0, &result, 1);
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) WASM_CHECK_I32(t, result.of.i32, 0);

    err = wasm_memory_read(m, 0, 80, seek_offset_bytes, sizeof(seek_offset_bytes));
    WASM_CHECK_OK(t, err);
    if (err == WASM_OK) {
        seek_offset = wasm__load_le64(seek_offset_bytes);
        WASM_CHECK_I64(t, (int64_t)seek_offset, 0);
    }

    err = wasm_call(m, "quit", NULL, 0, NULL, 0);
    WASM_CHECK_I32(t, (int32_t)err, (int32_t)WASM_ERR_EXIT);
    WASM_CHECK_I32(t, wasm_runtime_exit_code(&rt), 5);

    wasm_module_free(m);
    wasm_destroy(&rt);
}
#endif

#if WASM_ENABLE_PLATFORM
static const test_case platform_wasi_io_cases[] = {
    { "public api: WASI stubs bind fd_write, environ_get, and proc_exit", test_public_wasi_stub_helpers },
};
const test_suite platform_wasi_io_suite = { "platform.wasi_io", platform_wasi_io_cases, COUNTOF(platform_wasi_io_cases) };
#else
const test_suite platform_wasi_io_suite = { "platform.wasi_io", NULL, 0 };
#endif
