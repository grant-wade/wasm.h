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

typedef struct wasm2api_import_t {
	char* module_name;
	char* import_name;
	char* field_name;
	char* dispatch_name;
	wasm_import_kind_t kind;
	uint32_t resolved_index;
	uint32_t num_params;
	uint32_t num_results;
	wasm_valtype_t* params;
	wasm_valtype_t* results;
	wasm_valtype_t global_type;
	wasm_reftype_t global_ref_type;
	int global_is_mutable;
} wasm2api_import_t;

typedef struct wasm2api_model_t {
	char* api_prefix;
	wasm2api_func_t* funcs;
	uint32_t num_funcs;
	uint32_t skipped_funcs;
	wasm2api_import_t* imports;
	uint32_t num_imports;
	uint32_t num_func_imports;
	uint32_t num_global_imports;
} wasm2api_model_t;

typedef struct wasm2api_prescan_t {
	wasm_value_t** globals;
	uint32_t num_globals;
	uint32_t cap_globals;
} wasm2api_prescan_t;

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
	const char* backslash;

	if (!path) return "";
	slash = strrchr(path, '/');
	backslash = strrchr(path, '\\');
	if (!slash || (backslash && backslash > slash)) slash = backslash;
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

static int wasm2api_import_field_in_use(const wasm2api_model_t* model, const char* name) {
	uint32_t i;

	for (i = 0; i < model->num_imports; i++) {
		if (strcmp(model->imports[i].field_name, name) == 0) return 1;
	}
	return 0;
}

static char* wasm2api_make_unique_import_field_name(const wasm2api_model_t* model,
													const char* base) {
	uint32_t suffix = 2u;
	char* candidate = wasm2api_strdup(base);

	if (!candidate) return NULL;
	while (wasm2api_import_field_in_use(model, candidate)) {
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
		case WASM_TYPE_NOFUNC:
			return "nofunc";
		case WASM_TYPE_NOEXTERN:
			return "noextern";
		case WASM_TYPE_NONE:
			return "none";
		default:
			return "unsupported";
	}
}

static const char* wasm2api_wasm_type_enum(wasm_valtype_t type) {
	switch (type) {
		case WASM_TYPE_VOID:
			return "WASM_TYPE_VOID";
		case WASM_TYPE_I32:
			return "WASM_TYPE_I32";
		case WASM_TYPE_I64:
			return "WASM_TYPE_I64";
		case WASM_TYPE_F32:
			return "WASM_TYPE_F32";
		case WASM_TYPE_F64:
			return "WASM_TYPE_F64";
		case WASM_TYPE_EXTERNREF:
			return "WASM_TYPE_EXTERNREF";
		case WASM_TYPE_FUNCREF:
			return "WASM_TYPE_FUNCREF";
		case WASM_TYPE_V128:
			return "WASM_TYPE_V128";
		case WASM_TYPE_ANYREF:
			return "WASM_TYPE_ANYREF";
		case WASM_TYPE_EQREF:
			return "WASM_TYPE_EQREF";
		case WASM_TYPE_STRUCTREF:
			return "WASM_TYPE_STRUCTREF";
		case WASM_TYPE_ARRAYREF:
			return "WASM_TYPE_ARRAYREF";
		case WASM_TYPE_I31REF:
			return "WASM_TYPE_I31REF";
		case WASM_TYPE_NOFUNC:
			return "WASM_TYPE_NOFUNC";
		case WASM_TYPE_NOEXTERN:
			return "WASM_TYPE_NOEXTERN";
		case WASM_TYPE_NONE:
			return "WASM_TYPE_NONE";
		default:
			return "WASM_TYPE_VOID";
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

static char wasm2api_fmt_char(wasm_valtype_t type) {
	switch (type) {
		case WASM_TYPE_I32:
			return 'i';
		case WASM_TYPE_I64:
			return 'I';
		case WASM_TYPE_F32:
			return 'f';
		case WASM_TYPE_F64:
			return 'F';
		case WASM_TYPE_EXTERNREF:
			return 'r';
		default:
			return '?';
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

static void wasm2api_prescan_free(wasm2api_prescan_t* prescan) {
	uint32_t i;

	if (!prescan) return;
	for (i = 0; i < prescan->num_globals; i++) free(prescan->globals[i]);
	free(prescan->globals);
	memset(prescan, 0, sizeof(*prescan));
}

static wasm_value_t* wasm2api_prescan_add_global(wasm2api_prescan_t* prescan) {
	wasm_value_t** next;
	wasm_value_t* value;
	uint32_t new_cap;

	if (prescan->num_globals >= prescan->cap_globals) {
		new_cap = prescan->cap_globals ? prescan->cap_globals * 2u : 8u;
		next = (wasm_value_t**)realloc(prescan->globals, new_cap * sizeof(*next));
		if (!next) return NULL;
		prescan->globals = next;
		prescan->cap_globals = new_cap;
	}

	value = (wasm_value_t*)calloc(1u, sizeof(*value));
	if (!value) return NULL;
	prescan->globals[prescan->num_globals++] = value;
	return value;
}

static int wasm2api_prescan_read_valtype(wasm__reader_t* reader,
										  wasm_valtype_t* out_type,
										  wasm_reftype_t* out_ref_type) {
	wasm_valtype_t type;

	if (!reader || reader->ptr >= reader->end) return 0;
	if (wasm__is_reftype_lead_byte(*reader->ptr)) {
		if (wasm__read_reftype(NULL, reader, out_type, out_ref_type) != WASM_OK) return 0;
		return !reader->malformed;
	}

	type = (wasm_valtype_t)wasm__read_u8(reader);
	if (!wasm__is_value_type(type)) {
		reader->ptr = reader->end;
		reader->malformed = 1;
		return 0;
	}
	if (out_type) *out_type = type;
	if (out_ref_type) wasm__clear_reftype(out_ref_type);
	return 1;
}

static int wasm2api_prescan_init_global_value(wasm_valtype_t type,
										   const wasm_reftype_t* ref_type,
										   wasm_value_t* out_value) {
	memset(out_value, 0, sizeof(*out_value));
	out_value->type = type ? type : WASM_TYPE_ANYREF;
	if (wasm__is_ref_type(type) && ref_type && !ref_type->nullable) return 0;
	return 1;
}

static wasm_error_t wasm2api_placeholder_import(wasm_module_t* mod,
									const wasm_value_t* args,
									uint32_t num_args,
									wasm_value_t* results,
									uint32_t num_results,
									void* userdata) {
	(void)mod;
	(void)args;
	(void)num_args;
	(void)userdata;
	if (results && num_results > 0u) memset(results, 0, sizeof(*results) * num_results);
	return WASM_OK;
}

static int wasm2api_prescan_bind_imports(wasm_runtime_t* rt,
									const uint8_t* wasm_bytes,
									size_t wasm_len,
									wasm2api_prescan_t* prescan) {
	wasm__reader_t reader;

	if (!rt || !wasm_bytes || wasm_len < 8u) return 0;
	if (memcmp(wasm_bytes, "\0asm", 4u) != 0 ||
		memcmp(wasm_bytes + 4u, "\x01\0\0\0", 4u) != 0) {
		fprintf(stderr, "wasm2api: invalid wasm header\n");
		return 0;
	}

	reader.ptr = wasm_bytes + 8u;
	reader.end = wasm_bytes + wasm_len;
	reader.malformed = 0;

	while (reader.ptr < reader.end) {
		uint8_t section_id = wasm__read_u8(&reader);
		uint32_t section_len = wasm__read_leb128_u32(&reader);
		wasm__reader_t section_reader;

		if (reader.malformed || !wasm__has(&reader, section_len)) break;

		section_reader.ptr = reader.ptr;
		section_reader.end = reader.ptr + section_len;
		section_reader.malformed = 0;

		if (section_id == 2u) {
			uint32_t count = wasm__read_leb128_u32(&section_reader);
			uint32_t i;

			for (i = 0; i < count; i++) {
				char* module_name = NULL;
				char* import_name = NULL;
				uint8_t kind;
				wasm_error_t err;

				err = wasm__read_name_owned(&section_reader, &module_name);
				if (err != WASM_OK) goto fail;
				err = wasm__read_name_owned(&section_reader, &import_name);
				if (err != WASM_OK) {
					free(module_name);
					goto fail;
				}

				kind = wasm__read_u8(&section_reader);
				if (kind == 0x00u) {
					wasm_import_t import_binding;

					(void)wasm__read_leb128_u32(&section_reader);
					memset(&import_binding, 0, sizeof(import_binding));
					import_binding.module = module_name;
					import_binding.name = import_name;
					import_binding.func = wasm2api_placeholder_import;
					err = wasm_register_import(rt, &import_binding);
					free(module_name);
					free(import_name);
					if (err != WASM_OK) goto fail;
				} else if (kind == 0x01u) {
					uint8_t limits_flags;

					wasm__skip_reftype(&section_reader);
					limits_flags = wasm__read_u8(&section_reader);
					(void)wasm__read_leb128_u32(&section_reader);
					if ((limits_flags & 1u) != 0u) (void)wasm__read_leb128_u32(&section_reader);
					free(module_name);
					free(import_name);
				} else if (kind == 0x02u) {
					uint8_t limits_flags = wasm__read_u8(&section_reader);

					(void)wasm__read_leb128_u32(&section_reader);
					if ((limits_flags & 1u) != 0u) (void)wasm__read_leb128_u32(&section_reader);
					free(module_name);
					free(import_name);
				} else if (kind == 0x03u) {
					wasm_global_import_t global_binding;
					wasm_valtype_t type;
					wasm_reftype_t ref_type;
					uint8_t is_mutable;
					wasm_value_t* value;

					if (!wasm2api_prescan_read_valtype(&section_reader, &type, &ref_type)) {
						free(module_name);
						free(import_name);
						goto fail;
					}
					is_mutable = wasm__read_u8(&section_reader);
					value = wasm2api_prescan_add_global(prescan);
					if (!value) {
						free(module_name);
						free(import_name);
						goto fail;
					}
					if (!wasm2api_prescan_init_global_value(type, &ref_type, value)) {
						fprintf(stderr,
								"wasm2api: cannot inspect non-nullable global import %s.%s\n",
								module_name, import_name);
						free(module_name);
						free(import_name);
						goto fail;
					}
					memset(&global_binding, 0, sizeof(global_binding));
					global_binding.module = module_name;
					global_binding.name = import_name;
					global_binding.type = type;
					global_binding.ref_type = ref_type;
					global_binding.is_mutable = (int)is_mutable;
					global_binding.value = value;
					err = wasm_register_global_import(rt, &global_binding);
					free(module_name);
					free(import_name);
					if (err != WASM_OK) goto fail;
				} else if (kind == 0x04u) {
					if (wasm__read_u8(&section_reader) != 0x00u) {
						free(module_name);
						free(import_name);
						goto fail;
					}
					(void)wasm__read_leb128_u32(&section_reader);
					free(module_name);
					free(import_name);
				} else {
					free(module_name);
					free(import_name);
					goto fail;
				}

				if (section_reader.malformed) goto fail;
			}
		}

		if (section_reader.malformed || section_reader.ptr != section_reader.end) goto fail;
		reader.ptr = section_reader.end;
	}

	if (reader.malformed) goto fail;
	return 1;

fail:
	fprintf(stderr, "wasm2api: failed to pre-scan module imports\n");
	return 0;
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
	for (i = 0; i < model->num_imports; i++) {
		free(model->imports[i].module_name);
		free(model->imports[i].import_name);
		free(model->imports[i].field_name);
		free(model->imports[i].dispatch_name);
		free(model->imports[i].params);
		free(model->imports[i].results);
	}
	free(model->funcs);
	free(model->imports);
	memset(model, 0, sizeof(*model));
}

static int wasm2api_collect_funcs(wasm_module_t* mod, wasm2api_model_t* model) {
	uint32_t export_count;
	uint32_t export_index;

	export_count = wasm_export_count(mod);
	model->funcs = (wasm2api_func_t*)calloc(export_count ? export_count : 1u,
									 sizeof(*model->funcs));
	if (!model->funcs) return 0;

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
				model->skipped_funcs++;
				goto skip_export;
			}
		}

		for (result_index = 0; result_index < wasm_func_result_count(mod, func_idx); result_index++) {
			wasm_valtype_t type = wasm_func_result_type(mod, func_idx, result_index);
			if (!wasm2api_type_supported(type)) {
				fprintf(stderr,
						"wasm2api: skipping export '%s': unsupported result type %s\n",
						export_name, wasm2api_wasm_type_name(type));
				model->skipped_funcs++;
				goto skip_export;
			}
		}

		func = &model->funcs[model->num_funcs];
		memset(func, 0, sizeof(*func));
		func->export_name = wasm2api_strdup(export_name);
		if (!func->export_name) return 0;

		base_name = wasm2api_make_identifier(export_name);
		if (!base_name) return 0;
		unique_name = wasm2api_make_unique_func_name(model, base_name);
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

		model->num_funcs++;

	skip_export:
		;
	}

	return 1;
}

static int wasm2api_collect_imports(wasm_module_t* mod, wasm2api_model_t* model) {
	uint32_t import_count;
	uint32_t import_index;

	import_count = wasm_import_count(mod);
	model->imports = (wasm2api_import_t*)calloc(import_count ? import_count : 1u,
										 sizeof(*model->imports));
	if (!model->imports) return 0;

	for (import_index = 0; import_index < import_count; import_index++) {
		const wasm_import_info_t* info = wasm_import_info(mod, import_index);
		wasm2api_import_t* import_entry;
		char* module_ident;
		char* name_ident;
		char* base_field;

		if (!info) continue;
		if (info->kind != WASM_IMPORT_FUNC && info->kind != WASM_IMPORT_GLOBAL) continue;

		import_entry = &model->imports[model->num_imports];
		memset(import_entry, 0, sizeof(*import_entry));
		import_entry->module_name = wasm2api_strdup(info->module);
		import_entry->import_name = wasm2api_strdup(info->name);
		if (!import_entry->module_name || !import_entry->import_name) return 0;
		import_entry->kind = info->kind;
		import_entry->resolved_index = info->index;

		module_ident = wasm2api_make_identifier(info->module);
		name_ident = wasm2api_make_identifier(info->name);
		if (!module_ident || !name_ident) {
			free(module_ident);
			free(name_ident);
			return 0;
		}
		base_field = wasm2api_concat3(module_ident, "_", name_ident);
		free(module_ident);
		free(name_ident);
		if (!base_field) return 0;
		import_entry->field_name = wasm2api_make_unique_import_field_name(model, base_field);
		free(base_field);
		if (!import_entry->field_name) return 0;

		if (info->kind == WASM_IMPORT_FUNC) {
			uint32_t param_index;
			uint32_t result_index;

			import_entry->dispatch_name = wasm2api_concat3("dispatch_", import_entry->field_name, "");
			if (!import_entry->dispatch_name) return 0;
			import_entry->num_params = wasm_func_param_count(mod, info->index);
			import_entry->num_results = wasm_func_result_count(mod, info->index);

			for (param_index = 0; param_index < import_entry->num_params; param_index++) {
				wasm_valtype_t type = wasm_func_param_type(mod, info->index, param_index);
				if (!wasm2api_type_supported(type)) {
					fprintf(stderr,
							"wasm2api: unsupported imported function parameter type %s for %s.%s\n",
							wasm2api_wasm_type_name(type), info->module, info->name);
					return 0;
				}
			}

			for (result_index = 0; result_index < import_entry->num_results; result_index++) {
				wasm_valtype_t type = wasm_func_result_type(mod, info->index, result_index);
				if (!wasm2api_type_supported(type)) {
					fprintf(stderr,
							"wasm2api: unsupported imported function result type %s for %s.%s\n",
							wasm2api_wasm_type_name(type), info->module, info->name);
					return 0;
				}
			}

			if (import_entry->num_params > 0u) {
				import_entry->params = (wasm_valtype_t*)malloc(sizeof(*import_entry->params) * import_entry->num_params);
				if (!import_entry->params) return 0;
				for (param_index = 0; param_index < import_entry->num_params; param_index++)
					import_entry->params[param_index] = wasm_func_param_type(mod, info->index, param_index);
			}

			if (import_entry->num_results > 0u) {
				import_entry->results = (wasm_valtype_t*)malloc(sizeof(*import_entry->results) * import_entry->num_results);
				if (!import_entry->results) return 0;
				for (result_index = 0; result_index < import_entry->num_results; result_index++)
					import_entry->results[result_index] = wasm_func_result_type(mod, info->index, result_index);
			}

			model->num_func_imports++;
		} else {
			import_entry->global_type = info->type;
			import_entry->global_ref_type = info->ref_type;
			import_entry->global_is_mutable = info->is_mutable;
			model->num_global_imports++;
		}

		model->num_imports++;
	}

	return 1;
}

static int wasm2api_collect_model(wasm_module_t* mod, const char* api_prefix,
									  wasm2api_model_t* out_model) {
	memset(out_model, 0, sizeof(*out_model));
	out_model->api_prefix = wasm2api_make_identifier(wasm2api_basename(api_prefix));
	if (!out_model->api_prefix) return 0;
	if (!wasm2api_collect_funcs(mod, out_model)) return 0;
	if (!wasm2api_collect_imports(mod, out_model)) return 0;
	return 1;
}

static int wasm2api_model_has_import_bindings(const wasm2api_model_t* model) {
	return model->num_func_imports > 0u || model->num_global_imports > 0u;
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

static void wasm2api_emit_import_callback_field(FILE* out, const wasm2api_import_t* import_entry) {
	uint32_t i;
	int need_comma = 0;

	if (import_entry->num_results == 1u) {
		fprintf(out, "    %s (*%s)(", wasm2api_c_type(import_entry->results[0]),
				import_entry->field_name);
	} else {
		fprintf(out, "    void (*%s)(", import_entry->field_name);
	}

	for (i = 0; i < import_entry->num_params; i++) {
		if (need_comma) fputs(", ", out);
		fprintf(out, "%s arg%u", wasm2api_c_type(import_entry->params[i]), (unsigned)i);
		need_comma = 1;
	}
	if (import_entry->num_results > 1u) {
		for (i = 0; i < import_entry->num_results; i++) {
			if (need_comma) fputs(", ", out);
			fprintf(out, "%s* out%u", wasm2api_c_type(import_entry->results[i]), (unsigned)i);
			need_comma = 1;
		}
	}
	if (need_comma) fputs(", ", out);
	fputs("void* userdata);\n", out);
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

static void wasm2api_emit_value_expr(FILE* out, wasm_valtype_t type, const char* value_name) {
	switch (type) {
		case WASM_TYPE_I32:
			fprintf(out, "wasm_i32(%s)", value_name);
			break;
		case WASM_TYPE_I64:
			fprintf(out, "wasm_i64(%s)", value_name);
			break;
		case WASM_TYPE_F32:
			fprintf(out, "wasm_f32(%s)", value_name);
			break;
		case WASM_TYPE_F64:
			fprintf(out, "wasm_f64(%s)", value_name);
			break;
		case WASM_TYPE_EXTERNREF:
			fprintf(out, "wasm_externref((uintptr_t)%s)", value_name);
			break;
		default:
			fputs("wasm_i32(0)", out);
			break;
	}
}

static void wasm2api_emit_arg_read_expr(FILE* out, wasm_valtype_t type, uint32_t index) {
	switch (type) {
		case WASM_TYPE_I32:
			fprintf(out, "args[%u].of.i32", (unsigned)index);
			break;
		case WASM_TYPE_I64:
			fprintf(out, "args[%u].of.i64", (unsigned)index);
			break;
		case WASM_TYPE_F32:
			fprintf(out, "args[%u].of.f32", (unsigned)index);
			break;
		case WASM_TYPE_F64:
			fprintf(out, "args[%u].of.f64", (unsigned)index);
			break;
		case WASM_TYPE_EXTERNREF:
			fprintf(out, "(void*)args[%u].of.externref", (unsigned)index);
			break;
		default:
			fputs("0", out);
			break;
	}
}

static char* wasm2api_build_fmt_string(const wasm_valtype_t* params, uint32_t num_params,
									   const wasm_valtype_t* results, uint32_t num_results) {
	size_t len = (size_t)num_params + (size_t)(num_results ? num_results : 1u) + 3u;
	char* fmt = (char*)malloc(len);
	size_t offset = 0u;
	uint32_t i;

	if (!fmt) return NULL;
	for (i = 0; i < num_params; i++) fmt[offset++] = wasm2api_fmt_char(params[i]);
	fmt[offset++] = '(';
	if (num_results == 0u) {
		fmt[offset++] = 'v';
	} else {
		for (i = 0; i < num_results; i++) fmt[offset++] = wasm2api_fmt_char(results[i]);
	}
	fmt[offset++] = ')';
	fmt[offset] = '\0';
	return fmt;
}

static void wasm2api_emit_init_signature(FILE* out, const wasm2api_model_t* model,
									 int embed_wasm, int with_semicolon) {
	int has_imports = wasm2api_model_has_import_bindings(model);

	fprintf(out, "%s_ctx_t* %s_%s(", model->api_prefix, model->api_prefix,
			embed_wasm ? "init_embedded" : "init");
	fprintf(out, "wasm_runtime_t* rt");
	if (!embed_wasm) fputs(", const uint8_t* wasm_bytes, size_t len", out);
	if (has_imports) fprintf(out, ", const %s_imports_t* imports", model->api_prefix);
	fputs(")", out);
	if (with_semicolon) fputs(";\n", out);
}

static void wasm2api_emit_import_dispatcher(FILE* out, const wasm2api_model_t* model,
									const wasm2api_import_t* import_entry) {
	uint32_t i;

	fprintf(out,
			"static wasm_error_t %s_%s(wasm_module_t* mod, const wasm_value_t* args,\n"
			"                                  uint32_t num_args, wasm_value_t* results,\n"
			"                                  uint32_t num_results, void* userdata) {\n",
			model->api_prefix, import_entry->dispatch_name);
	fprintf(out, "    const %s_imports_t* imports = (const %s_imports_t*)userdata;\n",
			model->api_prefix, model->api_prefix);
	fputs("\n", out);
	fputs("    (void)mod;\n", out);
	if (import_entry->num_results == 1u) {
		fprintf(out, "    %s ret = %s;\n", wasm2api_c_type(import_entry->results[0]),
				wasm2api_default_return(import_entry->results[0]));
	} else if (import_entry->num_results > 1u) {
		for (i = 0; i < import_entry->num_results; i++) {
			fprintf(out, "    %s out%u = %s;\n", wasm2api_c_type(import_entry->results[i]),
					(unsigned)i, wasm2api_default_return(import_entry->results[i]));
		}
	}
	fprintf(out, "    if (!imports || !imports->%s) return WASM_ERR_UNKNOWN_IMPORT;\n",
			import_entry->field_name);
	fprintf(out, "    if (num_args != %u || num_results != %u) return WASM_ERR_TYPE_MISMATCH;\n",
			(unsigned)import_entry->num_params, (unsigned)import_entry->num_results);
	fputs("    if (num_args > 0u && !args) return WASM_ERR_MALFORMED;\n", out);
	fputs("    if (num_results > 0u && !results) return WASM_ERR_MALFORMED;\n", out);
	fputs("\n", out);
	fputs("    ", out);
	if (import_entry->num_results == 1u) fputs("ret = ", out);
	fprintf(out, "imports->%s(", import_entry->field_name);
	for (i = 0; i < import_entry->num_params; i++) {
		if (i > 0u) fputs(", ", out);
		wasm2api_emit_arg_read_expr(out, import_entry->params[i], i);
	}
	if (import_entry->num_results > 1u) {
		for (i = 0; i < import_entry->num_results; i++) {
			if (import_entry->num_params > 0u || i > 0u) fputs(", ", out);
			fprintf(out, "&out%u", (unsigned)i);
		}
	}
	if (import_entry->num_params > 0u || import_entry->num_results > 1u) fputs(", ", out);
	fputs("imports->userdata);\n", out);
	if (import_entry->num_results == 1u) {
		fputs("    results[0] = ", out);
		wasm2api_emit_value_expr(out, import_entry->results[0], "ret");
		fputs(";\n", out);
	} else if (import_entry->num_results > 1u) {
		for (i = 0; i < import_entry->num_results; i++) {
			char value_name[16];

			snprintf(value_name, sizeof(value_name), "out%u", (unsigned)i);
			fprintf(out, "    results[%u] = ", (unsigned)i);
			wasm2api_emit_value_expr(out, import_entry->results[i], value_name);
			fputs(";\n", out);
		}
	}
	fputs("    return WASM_OK;\n", out);
	fputs("}\n\n", out);
}

static int wasm2api_write_header(const char* header_path, const wasm2api_model_t* model,
									 int embed_wasm) {
	FILE* out = fopen(header_path, "w");
	uint32_t i;
	int has_imports = wasm2api_model_has_import_bindings(model);

	if (!out) {
		fprintf(stderr, "wasm2api: failed to open %s: %s\n", header_path, strerror(errno));
		return 0;
	}

	fputs("/* AUTO-GENERATED BY wasm2api. DO NOT EDIT. */\n", out);
	fputs("#pragma once\n\n", out);
	fputs("#include <stdint.h>\n", out);
	fputs("#include \"wasm.h\"\n\n", out);
	fprintf(out, "typedef struct %s_ctx_t %s_ctx_t;\n", model->api_prefix, model->api_prefix);
	if (has_imports) {
		fprintf(out, "typedef struct %s_imports_t {\n", model->api_prefix);
		for (i = 0; i < model->num_imports; i++) {
			const wasm2api_import_t* import_entry = &model->imports[i];

			if (import_entry->kind == WASM_IMPORT_FUNC) {
				wasm2api_emit_import_callback_field(out, import_entry);
			} else if (import_entry->kind == WASM_IMPORT_GLOBAL) {
				fprintf(out, "    wasm_value_t* %s;\n", import_entry->field_name);
			}
		}
		fputs("    void* userdata;\n", out);
		fprintf(out, "} %s_imports_t;\n", model->api_prefix);
	}
	fputs("\n", out);
	wasm2api_emit_init_signature(out, model, 0, 1);
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
		wasm2api_emit_init_signature(out, model, 1, 1);
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
	int has_imports = wasm2api_model_has_import_bindings(model);

	if (!out) {
		fprintf(stderr, "wasm2api: failed to open %s: %s\n", source_path, strerror(errno));
		return 0;
	}

	fputs("/* AUTO-GENERATED BY wasm2api. DO NOT EDIT. */\n", out);
	fputs("#include \"", out);
	fputs(header_include, out);
	fputs("\"\n", out);
	fputs("#include <stdio.h>\n", out);
	fputs("#include <stdlib.h>\n", out);
	fputs("#include <string.h>\n\n", out);

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

	for (i = 0; i < model->num_imports; i++) {
		if (model->imports[i].kind == WASM_IMPORT_FUNC)
			wasm2api_emit_import_dispatcher(out, model, &model->imports[i]);
	}

	wasm2api_emit_init_signature(out, model, 0, 0);
	fputs(" {\n", out);
	fprintf(out, "    %s_ctx_t* ctx;\n", model->api_prefix);
	if (has_imports) fputs("    wasm_error_t err;\n", out);
	fputs("\n", out);
	fputs("    if (!rt || !wasm_bytes) return NULL;\n", out);
	fputs("    ctx = (void*)calloc(1u, sizeof(*ctx));\n", out);
	fputs("    if (!ctx) return NULL;\n", out);
	fputs("    ctx->rt = rt;\n", out);
	if (has_imports) {
		fputs("    if (!imports) {\n", out);
		fprintf(out, "        %s_set_error(ctx, WASM_ERR_UNKNOWN_IMPORT, \"missing import bindings\");\n",
				model->api_prefix);
		fputs("        free(ctx);\n", out);
		fputs("        return NULL;\n", out);
		fputs("    }\n", out);
		for (i = 0; i < model->num_imports; i++) {
			const wasm2api_import_t* import_entry = &model->imports[i];
			char* message = wasm2api_concat3("missing import binding: ", import_entry->module_name, ".");
			char* full_message;

			if (!message) {
				fclose(out);
				return 0;
			}
			full_message = wasm2api_concat3(message, import_entry->import_name, "");
			free(message);
			if (!full_message) {
				fclose(out);
				return 0;
			}
			fprintf(out, "    if (!imports->%s) {\n", import_entry->field_name);
			fprintf(out, "        %s_set_error(ctx, WASM_ERR_UNKNOWN_IMPORT, ", model->api_prefix);
			wasm2api_write_c_string(out, full_message);
			fputs(");\n", out);
			fputs("        free(ctx);\n", out);
			fputs("        return NULL;\n", out);
			fputs("    }\n", out);
			free(full_message);
		}
		for (i = 0; i < model->num_imports; i++) {
			const wasm2api_import_t* import_entry = &model->imports[i];

			if (import_entry->kind == WASM_IMPORT_FUNC) {
				char* fmt = wasm2api_build_fmt_string(import_entry->params, import_entry->num_params,
											 import_entry->results, import_entry->num_results);
				if (!fmt) {
					fclose(out);
					return 0;
				}
				fputs("    err = wasm_bind_host_func(rt, ", out);
				wasm2api_write_c_string(out, import_entry->module_name);
				fputs(", ", out);
				wasm2api_write_c_string(out, import_entry->import_name);
				fputs(", ", out);
				wasm2api_write_c_string(out, fmt);
				fprintf(out, ", %s_%s, (void*)imports);\n", model->api_prefix,
						import_entry->dispatch_name);
				fputs("    if (err != WASM_OK) { free(ctx); return NULL; }\n", out);
				free(fmt);
			} else if (import_entry->kind == WASM_IMPORT_GLOBAL) {
				fputs("    {\n", out);
				fputs("        wasm_global_import_t global_imp;\n", out);
				fputs("        memset(&global_imp, 0, sizeof(global_imp));\n", out);
				fputs("        global_imp.module = ", out);
				wasm2api_write_c_string(out, import_entry->module_name);
				fputs(";\n", out);
				fputs("        global_imp.name = ", out);
				wasm2api_write_c_string(out, import_entry->import_name);
				fputs(";\n", out);
				fprintf(out, "        global_imp.type = %s;\n",
						wasm2api_wasm_type_enum(import_entry->global_type));
				fprintf(out, "        global_imp.ref_type.type = %s;\n",
						wasm2api_wasm_type_enum(import_entry->global_ref_type.type));
				fprintf(out, "        global_imp.ref_type.type_index = %u;\n",
						(unsigned)import_entry->global_ref_type.type_index);
				fprintf(out, "        global_imp.ref_type.has_type_index = %d;\n",
						import_entry->global_ref_type.has_type_index ? 1 : 0);
				fprintf(out, "        global_imp.ref_type.nullable = %d;\n",
						import_entry->global_ref_type.nullable ? 1 : 0);
				fprintf(out, "        global_imp.is_mutable = %d;\n",
						import_entry->global_is_mutable ? 1 : 0);
				fprintf(out, "        global_imp.value = imports->%s;\n", import_entry->field_name);
				fputs("        err = wasm_register_global_import(rt, &global_imp);\n", out);
				fputs("        if (err != WASM_OK) { free(ctx); return NULL; }\n", out);
				fputs("    }\n", out);
			}
		}
	}
	fputs("    ctx->mod = wasm_load(rt, wasm_bytes, len);\n", out);
	fputs("    if (!ctx->mod) {\n", out);
	fputs("        free(ctx);\n", out);
	fputs("        return NULL;\n", out);
	fputs("    }\n", out);
	fputs("    wasm_module_set_userdata(ctx->mod, ctx);\n", out);
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

		wasm2api_emit_init_signature(out, model, 1, 0);
		fputs(" {\n", out);
		fputs("    return ", out);
		fprintf(out, "%s_init(rt, %s_embedded_wasm_data, %s_embedded_wasm_len",
				model->api_prefix, model->api_prefix, model->api_prefix);
		if (has_imports) fputs(", imports", out);
		fputs(");\n", out);
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
				fputs("        return;\n", out);
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
	wasm2api_prescan_t prescan;
	wasm_error_t err;
	int embed_wasm = 0;
	int i;
	int ok = 0;

	memset(&model, 0, sizeof(model));
	memset(&prescan, 0, sizeof(prescan));

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

	if (!wasm2api_prescan_bind_imports(&rt, wasm_bytes, wasm_len, &prescan)) goto cleanup_runtime;

	mod = wasm_load(&rt, wasm_bytes, wasm_len);
	if (!mod) {
		fprintf(stderr, "wasm2api: failed to load %s: %s", input_path,
				wasm_error_string(rt.last_error));
		if (rt.error_msg[0]) fprintf(stderr, " (%s)", rt.error_msg);
		fputc('\n', stderr);
		goto cleanup_runtime;
	}

	if (!wasm2api_collect_model(mod, output_prefix, &model)) {
		fprintf(stderr, "wasm2api: failed to inspect module\n");
		goto cleanup_runtime;
	}

	if (!wasm2api_write_header(header_path, &model, embed_wasm)) goto cleanup_runtime;
	if (!wasm2api_write_source(source_path, header_include, &model, embed_wasm, wasm_bytes,
								  wasm_len))
		goto cleanup_runtime;

	fprintf(stdout,
			"generated %s and %s (%u wrapper%s, %u import binding%s, %u skipped export%s%s)\n",
			header_path, source_path,
			(unsigned)model.num_funcs, model.num_funcs == 1u ? "" : "s",
			(unsigned)(model.num_func_imports + model.num_global_imports),
			(model.num_func_imports + model.num_global_imports) == 1u ? "" : "s",
			(unsigned)model.skipped_funcs,
			model.skipped_funcs == 1u ? "" : "s",
			embed_wasm ? ", embedded wasm" : "");
	ok = 1;

cleanup_runtime:
	if (mod) wasm_free_module(mod);
	wasm_destroy(&rt);
	wasm2api_prescan_free(&prescan);

cleanup:
	wasm2api_free_model(&model);
	free(wasm_bytes);
	free(header_path);
	free(source_path);
	free(derived_prefix);
	return ok ? 0 : 1;
}
