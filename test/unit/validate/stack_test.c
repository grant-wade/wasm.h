#include "../support.h"

TEST(test_runtime_config_limits_operand_stack_validation) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_config_t cfg;
    uint32_t i;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        for (i = 0; i < 9; i++) {
            emit(&body, 0x41);
            emit_leb128_i32(&body, (int32_t)i);
        }
        for (i = 0; i < 8; i++) emit(&body, 0x1A);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_config_default(&cfg);
    cfg.max_stack_values = 8;
    wasm_init_config(&rt, &cfg);

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected load to fail when validator stack exceeds configured limit");
    CHECK_MSG(t, strstr(rt.error_msg, "operand stack overflow") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

TEST(test_runtime_config_limits_label_depth_validation) {
    wasm_builder_t mod = { 0 };
    wasm_runtime_t rt;
    wasm_module_t* m;
    wasm_config_t cfg;
    uint32_t i;

    emit_header(&mod);

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x60);
        emit_leb128_u32(&sec, 0);
        emit_leb128_u32(&sec, 1);
        emit(&sec, 0x7F);
        emit_section(&mod, 1, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&sec, 0);
        emit_section(&mod, 3, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        emit_leb128_u32(&sec, 1);
        emit_export_func(&sec, "run", 0);
        emit_section(&mod, 7, sec.buf, sec.len);
    }

    {
        wasm_builder_t sec = { 0 };
        wasm_builder_t body = { 0 };

        emit_leb128_u32(&sec, 1);
        emit_leb128_u32(&body, 0);
        for (i = 0; i < 4; i++) {
            emit(&body, 0x02);
            emit(&body, 0x40);
        }
        for (i = 0; i < 4; i++) emit(&body, 0x0B);
        emit(&body, 0x41);
        emit_leb128_i32(&body, 7);
        emit(&body, 0x0B);

        emit_leb128_u32(&sec, body.len);
        emit_bytes(&sec, body.buf, body.len);
        emit_section(&mod, 10, sec.buf, sec.len);
    }

    wasm_config_default(&cfg);
    cfg.max_labels = 4;
    wasm_init_config(&rt, &cfg);

    m = wasm_load(&rt, mod.buf, mod.len);
    CHECK_MSG(t, m == NULL, "%s", "expected load to fail when control depth exceeds configured limit");
    CHECK_MSG(t, strstr(rt.error_msg, "control stack overflow") != NULL, "%s", rt.error_msg);
    wasm_destroy(&rt);
}

static const test_case validate_stack_cases[] = {
    { "runtime config: validator stack limit is enforced at load", test_runtime_config_limits_operand_stack_validation },
    { "runtime config: validator label limit is enforced at load", test_runtime_config_limits_label_depth_validation },
};
const test_suite validate_stack_suite = { "validate.stack", validate_stack_cases, COUNTOF(validate_stack_cases) };
