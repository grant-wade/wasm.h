#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WASM_IMPL
#include "wasm.h"

typedef struct wasm2api_func_t {
	char* export_name;
	char* c_name;
	char* index_field;
	uint32_t func_idx;
	uint32_t num_params;
	uint32_t num_results;
	wasm_valtype_t* params;
	wasm_valtype_t* results;
} wasm2api_func_t;

typedef struct wasm2api_model_t {
	char* api_prefix;
	wasm2api_func_t* funcs;
	uint32_t num_funcs;
	uint32_t skipped_funcs;
} wasm2api_model_t;

static void wasm2api_usage(FILE* stream, const char* argv0) {
	fprintf(stream,
			"usage: %s [--embed] <input.wasm> [output_prefix]\n"
			"       %s [--embed] <input.wasm> -o <output_prefix>\n",
			argv0, argv0);
}

static char* wasm2api_strdup(const char* src) {
	size_t len;
	char* copy;

	if (!src) return NULL;
	len = strlen(src);
	copy = (char*)malloc(len + 1u);
	if (!copy) return NULL;
	memcpy(copy, src, len + 1u);
	return copy;
}

static char* wasm2api_concat3(const char* a, const char* b, const char* c) {
	size_t a_len = a ? strlen(a) : 0u;
	size_t b_len = b ? strlen(b) : 0u;
	size_t c_len = c ? strlen(c) : 0u;
	char* out = (char*)malloc(a_len + b_len + c_len + 1u);

	if (!out) return NULL;
	if (a_len) memcpy(out, a, a_len);
	if (b_len) memcpy(out + a_len, b, b_len);
	if (c_len) memcpy(out + a_len + b_len, c, c_len);
	out[a_len + b_len + c_len] = '\0';
	return out;
}

static const char* wasm2api_basename(const char* path) {
	const char* slash;

	if (!path) return "";
	slash = strrchr(path, '/');
	return slash ? slash + 1 : path;
}

static int wasm2api_is_c_keyword(const char* text) {
	static const char* keywords[] = {
		"auto",     "break",   "case",     "char",    "const",   "continue",
		"default",  "do",      "double",   "else",    "enum",    "extern",
		"float",    "for",     "goto",     "if",      "inline",  "int",
		"long",     "register", "restrict", "return",  "short",   "signed",
		"sizeof",   "static",  "struct",   "switch",  "typedef", "union",
		"unsigned", "void",    "volatile", "while",   "_Bool",   "_Complex",
		"_Imaginary"
	};
	size_t i;

	if (!text || !text[0]) return 0;

	for (i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
		if (strcmp(text, keywords[i]) == 0) return 1;
	}
	return 0;
}

static char* wasm2api_make_identifier(const char* text) {
	size_t in_len;
	size_t out_len = 0u;
	size_t i;
	char* out;

	if (!text || !text[0]) return wasm2api_strdup("item");

	in_len = strlen(text);
	out = (char*)malloc(in_len + 3u);
	if (!out) return NULL;

	for (i = 0; i < in_len; i++) {
		unsigned char ch = (unsigned char)text[i];

		if (isalnum(ch)) {
			if (out_len == 0u && isdigit(ch)) out[out_len++] = '_';
			out[out_len++] = (char)tolower(ch);
		} else {
			if (out_len == 0u || out[out_len - 1u] != '_') out[out_len++] = '_';
		}
	}

	while (out_len > 0u && out[out_len - 1u] == '_') out_len--;
	if (out_len == 0u) out[out_len++] = 'x';
	out[out_len] = '\0';

	if (wasm2api_is_c_keyword(out)) {
		out[out_len++] = '_';
		out[out_len] = '\0';
	}

	return out;
}

static int wasm2api_func_name_in_use(const wasm2api_model_t* model, const char* name) {
	uint32_t i;

	for (i = 0; i < model->num_funcs; i++) {
		if (strcmp(model->funcs[i].c_name, name) == 0) return 1;
	}
	return 0;
}

static char* wasm2api_make_unique_func_name(const wasm2api_model_t* model, const char* base) {
	uint32_t suffix = 2u;
	char* candidate = wasm2api_strdup(base);

	if (!candidate) return NULL;
	while (wasm2api_func_name_in_use(model, candidate)) {
		char suffix_buf[32];
		char* next;

		free(candidate);
		snprintf(suffix_buf, sizeof(suffix_buf), "_%u", (unsigned)suffix++);
		next = wasm2api_concat3(base, suffix_buf, "");
		if (!next) return NULL;
		candidate = next;
	}

	return candidate;
}

static const char* wasm2api_wasm_type_name(wasm_valtype_t type) {
	switch (type) {
		case WASM_TYPE_I32:
			return "i32";
		case WASM_TYPE_I64:
			return "i64";
		case WASM_TYPE_F32:
			return "f32";
		case WASM_TYPE_F64:
			return "f64";
		case WASM_TYPE_EXTERNREF:
			return "externref";
		case WASM_TYPE_FUNCREF:
			return "funcref";
		case WASM_TYPE_V128:
			return "v128";
		case WASM_TYPE_ANYREF:
			return "anyref";
		case WASM_TYPE_EQREF:
			return "eqref";
		case WASM_TYPE_STRUCTREF:
			return "structref";
		case WASM_TYPE_ARRAYREF:
			return "arrayref";
		case WASM_TYPE_I31REF:
			return "i31ref";
		default:
			return "unsupported";
	}
}

static int wasm2api_type_supported(wasm_valtype_t type) {
	return type == WASM_TYPE_I32 || type == WASM_TYPE_I64 || type == WASM_TYPE_F32 ||
		   type == WASM_TYPE_F64 || type == WASM_TYPE_EXTERNREF;
}

static const char* wasm2api_c_type(wasm_valtype_t type) {
	switch (type) {
		case WASM_TYPE_I32:
			return "int32_t";
		case WASM_TYPE_I64:
			return "int64_t";
		case WASM_TYPE_F32:
			return "float";
		case WASM_TYPE_F64:
			return "double";
		case WASM_TYPE_EXTERNREF:
			return "void*";
		default:
			return NULL;
	}
}

static const char* wasm2api_default_return(wasm_valtype_t type) {
	switch (type) {
		case WASM_TYPE_I32:
			return "0";
		case WASM_TYPE_I64:
			return "0";
		case WASM_TYPE_F32:
			return "0.0f";
		case WASM_TYPE_F64:
			return "0.0";
		case WASM_TYPE_EXTERNREF:
			return "NULL";
		default:
			return "0";
	}
}

static int wasm2api_read_file(const char* path, uint8_t** out_bytes, size_t* out_len) {
	FILE* file;
	long size;
	uint8_t* bytes;

	*out_bytes = NULL;
	*out_len = 0u;

	file = fopen(path, "rb");
	if (!file) {
		fprintf(stderr, "wasm2api: failed to open %s: %s\n", path, strerror(errno));
		return 0;
	}
	if (fseek(file, 0, SEEK_END) != 0) {
		fprintf(stderr, "wasm2api: failed to seek %s\n", path);
		fclose(file);
		return 0;
	}
	size = ftell(file);
	if (size < 0) {
		fprintf(stderr, "wasm2api: failed to stat %s\n", path);
		fclose(file);
		return 0;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		fprintf(stderr, "wasm2api: failed to rewind %s\n", path);
		fclose(file);
		return 0;
	}

	bytes = (uint8_t*)malloc((size_t)size ? (size_t)size : 1u);
	if (!bytes) {
		fprintf(stderr, "wasm2api: out of memory reading %s\n", path);
		fclose(file);
		return 0;
	}
	if ((size_t)size > 0u && fread(bytes, 1u, (size_t)size, file) != (size_t)size) {
		fprintf(stderr, "wasm2api: failed to read %s\n", path);
		free(bytes);
		fclose(file);
		return 0;
	}

	fclose(file);
	*out_bytes = bytes;
	*out_len = (size_t)size;
	return 1;
}

static char* wasm2api_default_output_prefix(const char* input_path) {
	const char* base = wasm2api_basename(input_path);
	const char* dot = strrchr(base, '.');
	size_t stem_len = dot && dot != base ? (size_t)(dot - base) : strlen(base);
	char* stem;
	char* ident;

	stem = (char*)malloc(stem_len + 1u);
	if (!stem) return NULL;
	memcpy(stem, base, stem_len);
	stem[stem_len] = '\0';

	ident = wasm2api_make_identifier(stem);
	free(stem);
	return ident;
}

static void wasm2api_free_model(wasm2api_model_t* model) {
	uint32_t i;

	if (!model) return;

	free(model->api_prefix);
	for (i = 0; i < model->num_funcs; i++) {
		free(model->funcs[i].export_name);
		free(model->funcs[i].c_name);
		free(model->funcs[i].index_field);
		free(model->funcs[i].params);
		free(model->funcs[i].results);
	}
	free(model->funcs);
	memset(model, 0, sizeof(*model));
}

static int wasm2api_collect_funcs(wasm_module_t* mod, const char* api_prefix,
								  wasm2api_model_t* out_model) {
	uint32_t export_count;
	uint32_t export_index;

	memset(out_model, 0, sizeof(*out_model));
	out_model->api_prefix = wasm2api_make_identifier(wasm2api_basename(api_prefix));
	if (!out_model->api_prefix) return 0;

	export_count = wasm_export_count(mod);
	out_model->funcs = (wasm2api_func_t*)calloc(export_count ? export_count : 1u,
												sizeof(*out_model->funcs));
	if (!out_model->funcs) return 0;

	for (export_index = 0; export_index < export_count; export_index++) {
		const char* export_name;
		wasm2api_func_t* func;
		uint32_t func_idx;
		uint32_t param_index;
		uint32_t result_index;
		char* base_name;
		char* unique_name;

		if (wasm_export_kind(mod, export_index) != WASM_EXPORT_FUNC) continue;

		export_name = wasm_export_name(mod, export_index);
		func_idx = wasm_export_index(mod, export_index);

		for (param_index = 0; param_index < wasm_func_param_count(mod, func_idx); param_index++) {
			wasm_valtype_t type = wasm_func_param_type(mod, func_idx, param_index);
			if (!wasm2api_type_supported(type)) {
				fprintf(stderr,
						"wasm2api: skipping export '%s': unsupported parameter type %s\n",
						export_name, wasm2api_wasm_type_name(type));
				out_model->skipped_funcs++;
				goto skip_export;
			}
		}

		for (result_index = 0; result_index < wasm_func_result_count(mod, func_idx); result_index++) {
			wasm_valtype_t type = wasm_func_result_type(mod, func_idx, result_index);
			if (!wasm2api_type_supported(type)) {
				fprintf(stderr,
						"wasm2api: skipping export '%s': unsupported result type %s\n",
						export_name, wasm2api_wasm_type_name(type));
				out_model->skipped_funcs++;
				goto skip_export;
			}
		}

		func = &out_model->funcs[out_model->num_funcs];
		memset(func, 0, sizeof(*func));
		func->export_name = wasm2api_strdup(export_name);
		if (!func->export_name) return 0;

		base_name = wasm2api_make_identifier(export_name);
		if (!base_name) return 0;
		unique_name = wasm2api_make_unique_func_name(out_model, base_name);
		free(base_name);
		if (!unique_name) return 0;
		func->c_name = unique_name;
		func->index_field = wasm2api_concat3("idx_", func->c_name, "");
		if (!func->index_field) return 0;

		func->func_idx = func_idx;
		func->num_params = wasm_func_param_count(mod, func_idx);
		func->num_results = wasm_func_result_count(mod, func_idx);

		if (func->num_params > 0u) {
			func->params = (wasm_valtype_t*)malloc(sizeof(*func->params) * func->num_params);
			if (!func->params) return 0;
			for (param_index = 0; param_index < func->num_params; param_index++)
				func->params[param_index] = wasm_func_param_type(mod, func_idx, param_index);
		}

		if (func->num_results > 0u) {
			func->results = (wasm_valtype_t*)malloc(sizeof(*func->results) * func->num_results);
			if (!func->results) return 0;
			for (result_index = 0; result_index < func->num_results; result_index++)
				func->results[result_index] = wasm_func_result_type(mod, func_idx, result_index);
		}

		out_model->num_funcs++;

	skip_export:
		;
	}

	return 1;
}

static void wasm2api_write_c_string(FILE* out, const char* text) {
	const unsigned char* p = (const unsigned char*)text;

	fputc('"', out);
	while (*p) {
		switch (*p) {
			case '\\':
				fputs("\\\\", out);
				break;
			case '"':
				fputs("\\\"", out);
				break;
			case '\n':
				fputs("\\n", out);
				break;
			case '\r':
				fputs("\\r", out);
				break;
			case '\t':
				fputs("\\t", out);
				break;
			default:
				if (*p < 0x20u || *p > 0x7Eu) {
					fprintf(out, "\\x%02X", (unsigned)*p);
				} else {
					fputc((int)*p, out);
				}
				break;
		}
		p++;
	}
	fputc('"', out);
}

static void wasm2api_write_byte_array(FILE* out, const uint8_t* bytes, size_t len) {
	size_t i;

	if (!len) {
		fputs("    0x00\n", out);
		return;
	}

	for (i = 0; i < len; i++) {
		if (i % 12u == 0u) fputs("    ", out);
		fprintf(out, "0x%02X", (unsigned)bytes[i]);
		if (i + 1u < len) fputs(", ", out);
		if ((i % 12u) == 11u || i + 1u == len) fputc('\n', out);
	}
}

static void wasm2api_emit_function_signature(FILE* out, const wasm2api_model_t* model,
											 const wasm2api_func_t* func, int with_semicolon) {
	uint32_t i;

	if (func->num_results == 1u) {
		fprintf(out, "%s ", wasm2api_c_type(func->results[0]));
	} else {
		fputs("void ", out);
	}

	fprintf(out, "%s_%s(%s_ctx_t* ctx", model->api_prefix, func->c_name, model->api_prefix);
	for (i = 0; i < func->num_params; i++) {
		fprintf(out, ", %s arg%u", wasm2api_c_type(func->params[i]), (unsigned)i);
	}
	if (func->num_results > 1u) {
		for (i = 0; i < func->num_results; i++) {
			fprintf(out, ", %s* out%u", wasm2api_c_type(func->results[i]), (unsigned)i);
		}
	}
	fputs(")", out);
	if (with_semicolon) fputs(";\n", out);
}

static void wasm2api_emit_pack_arg(FILE* out, wasm_valtype_t type, uint32_t index) {
	switch (type) {
		case WASM_TYPE_I32:
			fprintf(out, "    args[%u] = wasm_i32(arg%u);\n", (unsigned)index, (unsigned)index);
			break;
		case WASM_TYPE_I64:
			fprintf(out, "    args[%u] = wasm_i64(arg%u);\n", (unsigned)index, (unsigned)index);
			break;
		case WASM_TYPE_F32:
			fprintf(out, "    args[%u] = wasm_f32(arg%u);\n", (unsigned)index, (unsigned)index);
			break;
		case WASM_TYPE_F64:
			fprintf(out, "    args[%u] = wasm_f64(arg%u);\n", (unsigned)index, (unsigned)index);
			break;
		case WASM_TYPE_EXTERNREF:
			fprintf(out, "    args[%u] = wasm_externref((uintptr_t)arg%u);\n", (unsigned)index,
					(unsigned)index);
			break;
		default:
			break;
	}
}

static void wasm2api_emit_result_write(FILE* out, wasm_valtype_t type, uint32_t index,
									   const char* target_expr) {
	switch (type) {
		case WASM_TYPE_I32:
			fprintf(out, "    %s = results[%u].of.i32;\n", target_expr, (unsigned)index);
			break;
		case WASM_TYPE_I64:
			fprintf(out, "    %s = results[%u].of.i64;\n", target_expr, (unsigned)index);
			break;
		case WASM_TYPE_F32:
			fprintf(out, "    %s = results[%u].of.f32;\n", target_expr, (unsigned)index);
			break;
		case WASM_TYPE_F64:
			fprintf(out, "    %s = results[%u].of.f64;\n", target_expr, (unsigned)index);
			break;
		case WASM_TYPE_EXTERNREF:
			fprintf(out, "    %s = (void*)results[%u].of.externref;\n", target_expr,
					(unsigned)index);
			break;
		default:
			break;
	}
}

static int wasm2api_write_header(const char* header_path,
							 const wasm2api_model_t* model,
							 int embed_wasm) {
	FILE* out = fopen(header_path, "w");
	uint32_t i;

	if (!out) {
		fprintf(stderr, "wasm2api: failed to open %s: %s\n", header_path, strerror(errno));
		return 0;
	}

	fputs("/* AUTO-GENERATED BY wasm2api. DO NOT EDIT. */\n", out);
	fputs("#pragma once\n\n", out);
	fputs("#include <stdint.h>\n", out);
	fputs("#include \"wasm.h\"\n\n", out);
	fprintf(out, "typedef struct %s_ctx_t %s_ctx_t;\n\n", model->api_prefix, model->api_prefix);
	fprintf(out, "%s_ctx_t* %s_init(wasm_runtime_t* rt, const uint8_t* wasm_bytes, size_t len);\n",
			model->api_prefix, model->api_prefix);
	fprintf(out, "void %s_free(%s_ctx_t* ctx);\n", model->api_prefix, model->api_prefix);
	fprintf(out, "wasm_module_t* %s_module(%s_ctx_t* ctx);\n", model->api_prefix,
			model->api_prefix);
	fprintf(out, "wasm_error_t %s_last_error(const %s_ctx_t* ctx);\n", model->api_prefix,
			model->api_prefix);
	fprintf(out, "const char* %s_last_error_string(const %s_ctx_t* ctx);\n", model->api_prefix,
			model->api_prefix);
	fprintf(out, "const char* %s_last_error_message(const %s_ctx_t* ctx);\n", model->api_prefix,
			model->api_prefix);
	if (embed_wasm) {
		fprintf(out, "const uint8_t* %s_embedded_wasm(size_t* out_len);\n", model->api_prefix);
		fprintf(out, "%s_ctx_t* %s_init_embedded(wasm_runtime_t* rt);\n", model->api_prefix,
				model->api_prefix);
	}

	if (model->num_funcs > 0u) fputs("\n", out);
	for (i = 0; i < model->num_funcs; i++) {
		wasm2api_emit_function_signature(out, model, &model->funcs[i], 1);
	}

	if (fclose(out) != 0) {
		fprintf(stderr, "wasm2api: failed to write %s\n", header_path);
		return 0;
	}
	return 1;
}

static int wasm2api_write_source(const char* source_path,
							 const char* header_include,
							 const wasm2api_model_t* model,
							 int embed_wasm,
							 const uint8_t* wasm_bytes,
							 size_t wasm_len) {
	FILE* out = fopen(source_path, "w");
	uint32_t i;

	if (!out) {
		fprintf(stderr, "wasm2api: failed to open %s: %s\n", source_path, strerror(errno));
		return 0;
	}

	fputs("/* AUTO-GENERATED BY wasm2api. DO NOT EDIT. */\n", out);
	fputs("#include \"", out);
	fputs(header_include, out);
	fputs("\"\n", out);
	fputs("#include <stdio.h>\n", out);
	fputs("#include <stdlib.h>\n\n", out);

	if (embed_wasm) {
		fprintf(out, "static const uint8_t %s_embedded_wasm_data[] = {\n", model->api_prefix);
		wasm2api_write_byte_array(out, wasm_bytes, wasm_len);
		fputs("};\n", out);
		fprintf(out, "static const size_t %s_embedded_wasm_len = %zu;\n\n", model->api_prefix,
				wasm_len);
	}

	fprintf(out, "struct %s_ctx_t {\n", model->api_prefix);
	fputs("    wasm_runtime_t* rt;\n", out);
	fputs("    wasm_module_t* mod;\n", out);
	for (i = 0; i < model->num_funcs; i++) {
		fprintf(out, "    uint32_t %s;\n", model->funcs[i].index_field);
	}
	fputs("};\n\n", out);

	fprintf(out,
			"static void %s_set_error(%s_ctx_t* ctx, wasm_error_t err, const char* message) {\n",
			model->api_prefix, model->api_prefix);
	fputs("    if (!ctx || !ctx->rt) return;\n", out);
	fputs("    ctx->rt->last_error = err;\n", out);
	fputs("    snprintf(ctx->rt->error_msg, sizeof(ctx->rt->error_msg), \"%s\",\n", out);
	fputs("             message ? message : \"\");\n", out);
	fputs("}\n\n", out);

	fprintf(out, "static void %s_clear_error(%s_ctx_t* ctx) {\n", model->api_prefix,
			model->api_prefix);
	fputs("    if (!ctx || !ctx->rt) return;\n", out);
	fputs("    ctx->rt->last_error = WASM_OK;\n", out);
	fputs("    ctx->rt->error_msg[0] = '\\0';\n", out);
	fputs("}\n\n", out);

	fprintf(out, "%s_ctx_t* %s_init(wasm_runtime_t* rt, const uint8_t* wasm_bytes, size_t len) {\n",
			model->api_prefix, model->api_prefix);
	fprintf(out, "    %s_ctx_t* ctx;\n\n", model->api_prefix);
	fputs("    if (!rt || !wasm_bytes) return NULL;\n", out);
	fputs("    ctx = (void*)calloc(1u, sizeof(*ctx));\n", out);
	fputs("    if (!ctx) return NULL;\n", out);
	fputs("    ctx->rt = rt;\n", out);
	fputs("    ctx->mod = wasm_load(rt, wasm_bytes, len);\n", out);
	fputs("    if (!ctx->mod) {\n", out);
	fputs("        free(ctx);\n", out);
	fputs("        return NULL;\n", out);
	fputs("    }\n", out);
	for (i = 0; i < model->num_funcs; i++) {
		fputs("    if (!wasm_find_export(ctx->mod, ", out);
		wasm2api_write_c_string(out, model->funcs[i].export_name);
		fprintf(out,
				", NULL, &ctx->%s)) {\n"
				"        %s_set_error(ctx, WASM_ERR_UNDEFINED_EXPORT, \"cached export missing\");\n"
				"        wasm_free_module(ctx->mod);\n"
				"        free(ctx);\n"
				"        return NULL;\n"
				"    }\n",
				model->funcs[i].index_field, model->api_prefix);
	}
	fprintf(out, "    %s_clear_error(ctx);\n", model->api_prefix);
	fputs("    return ctx;\n", out);
	fputs("}\n\n", out);

	fprintf(out, "void %s_free(%s_ctx_t* ctx) {\n", model->api_prefix, model->api_prefix);
	fputs("    if (!ctx) return;\n", out);
	fputs("    if (ctx->mod) wasm_free_module(ctx->mod);\n", out);
	fputs("    free(ctx);\n", out);
	fputs("}\n\n", out);

	fprintf(out, "wasm_module_t* %s_module(%s_ctx_t* ctx) {\n", model->api_prefix,
			model->api_prefix);
	fputs("    return ctx ? ctx->mod : NULL;\n", out);
	fputs("}\n\n", out);

	fprintf(out, "wasm_error_t %s_last_error(const %s_ctx_t* ctx) {\n", model->api_prefix,
			model->api_prefix);
	fputs("    if (!ctx || !ctx->rt) return WASM_ERR_MALFORMED;\n", out);
	fputs("    return ctx->rt->last_error;\n", out);
	fputs("}\n\n", out);

	fprintf(out, "const char* %s_last_error_string(const %s_ctx_t* ctx) {\n", model->api_prefix,
			model->api_prefix);
	fprintf(out, "    return wasm_error_string(%s_last_error(ctx));\n", model->api_prefix);
	fputs("}\n\n", out);

	fprintf(out, "const char* %s_last_error_message(const %s_ctx_t* ctx) {\n", model->api_prefix,
			model->api_prefix);
	fputs("    if (!ctx || !ctx->rt) return \"invalid context\";\n", out);
	fputs("    return ctx->rt->error_msg;\n", out);
	fputs("}\n\n", out);

	if (embed_wasm) {
		fprintf(out, "const uint8_t* %s_embedded_wasm(size_t* out_len) {\n", model->api_prefix);
		fprintf(out, "    if (out_len) *out_len = %s_embedded_wasm_len;\n", model->api_prefix);
		fprintf(out, "    return %s_embedded_wasm_data;\n", model->api_prefix);
		fputs("}\n\n", out);

		fprintf(out, "%s_ctx_t* %s_init_embedded(wasm_runtime_t* rt) {\n", model->api_prefix,
				model->api_prefix);
		fprintf(out, "    return %s_init(rt, %s_embedded_wasm_data, %s_embedded_wasm_len);\n",
				model->api_prefix, model->api_prefix, model->api_prefix);
		fputs("}\n\n", out);
	}

	for (i = 0; i < model->num_funcs; i++) {
		const wasm2api_func_t* func = &model->funcs[i];
		uint32_t param_index;
		uint32_t result_index;

		wasm2api_emit_function_signature(out, model, func, 0);
		fputs(" {\n", out);
		if (func->num_results == 1u) {
			fprintf(out, "    %s ret = %s;\n", wasm2api_c_type(func->results[0]),
					wasm2api_default_return(func->results[0]));
		}
		fputs("    wasm_error_t err;\n", out);
		if (func->num_params > 0u) {
			fprintf(out, "    wasm_value_t args[%u];\n", (unsigned)func->num_params);
		}
		if (func->num_results > 0u) {
			fprintf(out, "    wasm_value_t results[%u];\n", (unsigned)func->num_results);
		}
		fputs("\n", out);

		if (func->num_results == 1u) {
			fputs("    if (!ctx || !ctx->mod) return ret;\n", out);
		} else {
			fputs("    if (!ctx || !ctx->mod) return;\n", out);
		}
		if (func->num_results > 1u) {
			for (result_index = 0; result_index < func->num_results; result_index++) {
				fprintf(out,
						"    if (!out%u) {\n"
						"        %s_set_error(ctx, WASM_ERR_MALFORMED, \"null output pointer\");\n",
						(unsigned)result_index, model->api_prefix);
				if (func->num_results == 1u) {
					fputs("        return ret;\n", out);
				} else {
					fputs("        return;\n", out);
				}
				fputs("    }\n", out);
			}
		}
		for (param_index = 0; param_index < func->num_params; param_index++) {
			wasm2api_emit_pack_arg(out, func->params[param_index], param_index);
		}
		fprintf(out,
				"    err = wasm_call_index(ctx->mod, ctx->%s, %s, %u, %s, %u);\n",
				func->index_field,
				func->num_params > 0u ? "args" : "NULL",
				(unsigned)func->num_params,
				func->num_results > 0u ? "results" : "NULL",
				(unsigned)func->num_results);

		if (func->num_results == 1u) {
			fputs("    if (err != WASM_OK) return ret;\n", out);
			wasm2api_emit_result_write(out, func->results[0], 0u, "ret");
			fprintf(out, "    %s_clear_error(ctx);\n", model->api_prefix);
			fputs("    return ret;\n", out);
		} else {
			fputs("    if (err != WASM_OK) return;\n", out);
			for (result_index = 0; result_index < func->num_results; result_index++) {
				char target[32];

				snprintf(target, sizeof(target), "*out%u", (unsigned)result_index);
				wasm2api_emit_result_write(out, func->results[result_index], result_index, target);
			}
			fprintf(out, "    %s_clear_error(ctx);\n", model->api_prefix);
			fputs("    return;\n", out);
		}
		fputs("}\n\n", out);
	}

	if (fclose(out) != 0) {
		fprintf(stderr, "wasm2api: failed to write %s\n", source_path);
		return 0;
	}
	return 1;
}

int main(int argc, char** argv) {
	const char* input_path = NULL;
	char* output_prefix = NULL;
	char* derived_prefix = NULL;
	char* header_path = NULL;
	char* source_path = NULL;
	const char* header_include;
	uint8_t* wasm_bytes = NULL;
	size_t wasm_len = 0u;
	wasm_runtime_t rt;
	wasm_module_t* mod = NULL;
	wasm2api_model_t model;
	wasm_error_t err;
	int embed_wasm = 0;
	int i;
	int ok = 0;

	memset(&model, 0, sizeof(model));

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
			wasm2api_usage(stdout, argv[0]);
			return 0;
		}
		if (strcmp(argv[i], "--embed") == 0) {
			embed_wasm = 1;
			continue;
		}
		if (strcmp(argv[i], "-o") == 0) {
			if (i + 1 >= argc) {
				wasm2api_usage(stderr, argv[0]);
				return 1;
			}
			output_prefix = argv[++i];
			continue;
		}
		if (!input_path) {
			input_path = argv[i];
			continue;
		}
		if (!output_prefix) {
			output_prefix = argv[i];
			continue;
		}

		wasm2api_usage(stderr, argv[0]);
		return 1;
	}

	if (!input_path) {
		wasm2api_usage(stderr, argv[0]);
		return 1;
	}
	if (!output_prefix) {
		derived_prefix = wasm2api_default_output_prefix(input_path);
		if (!derived_prefix) {
			fprintf(stderr, "wasm2api: failed to allocate output prefix\n");
			goto cleanup;
		}
		output_prefix = derived_prefix;
	}

	header_path = wasm2api_concat3(output_prefix, ".h", "");
	source_path = wasm2api_concat3(output_prefix, ".c", "");
	if (!header_path || !source_path) {
		fprintf(stderr, "wasm2api: failed to allocate output paths\n");
		goto cleanup;
	}
	header_include = wasm2api_basename(header_path);

	if (!wasm2api_read_file(input_path, &wasm_bytes, &wasm_len)) goto cleanup;

	err = wasm_init(&rt, NULL);
	if (err != WASM_OK) {
		fprintf(stderr, "wasm2api: runtime init failed: %s\n", wasm_error_string(err));
		goto cleanup;
	}

	mod = wasm_load(&rt, wasm_bytes, wasm_len);
	if (!mod) {
		fprintf(stderr, "wasm2api: failed to load %s: %s", input_path,
				wasm_error_string(rt.last_error));
		if (rt.error_msg[0]) fprintf(stderr, " (%s)", rt.error_msg);
		fputc('\n', stderr);
		goto cleanup_runtime;
	}

	if (!wasm2api_collect_funcs(mod, output_prefix, &model)) {
		fprintf(stderr, "wasm2api: failed to inspect exports\n");
		goto cleanup_runtime;
	}

	if (!wasm2api_write_header(header_path, &model, embed_wasm)) goto cleanup_runtime;
	if (!wasm2api_write_source(source_path, header_include, &model, embed_wasm, wasm_bytes,
							  wasm_len))
		goto cleanup_runtime;

	fprintf(stdout,
			"generated %s and %s (%u wrapper%s, %u skipped%s)\n",
			header_path, source_path,
			(unsigned)model.num_funcs, model.num_funcs == 1u ? "" : "s",
			(unsigned)model.skipped_funcs,
			embed_wasm ? ", embedded wasm" : "");
	ok = 1;

cleanup_runtime:
	if (mod) wasm_free_module(mod);
	wasm_destroy(&rt);

cleanup:
	wasm2api_free_model(&model);
	free(wasm_bytes);
	free(header_path);
	free(source_path);
	free(derived_prefix);
	return ok ? 0 : 1;
}
