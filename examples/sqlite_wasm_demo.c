#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WASM_IMPL
#include "sqlite_wasm.h"

#ifndef SQLITE_WASM_PATH
#error SQLITE_WASM_PATH must be defined by the build.
#endif

#define SQLITE_OK 0
#define SQLITE_ROW 100
#define SQLITE_DONE 101

#define SQLITE_INTEGER 1
#define SQLITE_FLOAT 2
#define SQLITE_TEXT 3
#define SQLITE_NULL 5

static int report_wrapper_error(const char* prefix) {
	fprintf(stderr, "%s: %s (%s)\n",
			prefix,
			sqlite_wasm_get_last_error_string(),
			sqlite_wasm_get_last_error_message());
	return 1;
}

static int read_file_bytes(const char* path, uint8_t** out_bytes, size_t* out_len) {
	FILE* file;
	long size;
	uint8_t* bytes;

	if (!path || !out_bytes || !out_len) return 0;
	*out_bytes = NULL;
	*out_len = 0u;

	file = fopen(path, "rb");
	if (!file) return 0;
	if (fseek(file, 0, SEEK_END) != 0) {
		fclose(file);
		return 0;
	}
	size = ftell(file);
	if (size < 0) {
		fclose(file);
		return 0;
	}
	if (fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return 0;
	}

	bytes = (uint8_t*)malloc((size_t)size > 0u ? (size_t)size : 1u);
	if (!bytes) {
		fclose(file);
		return 0;
	}
	if ((size_t)size > 0u && fread(bytes, 1u, (size_t)size, file) != (size_t)size) {
		free(bytes);
		fclose(file);
		return 0;
	}

	fclose(file);
	*out_bytes = bytes;
	*out_len = (size_t)size;
	return 1;
}

static wasm_module_t* sqlite_module(void) {
	return sqlite_wasm_get_module();
}

static int guest_write_bytes(uint32_t ptr, const void* src, size_t len) {
	wasm_module_t* mod = sqlite_module();

	if (!mod) return 0;
	return wasm_memory_write(mod, 0, ptr, src, len) == WASM_OK;
}

static int guest_read_bytes(uint32_t ptr, void* dst, size_t len) {
	wasm_module_t* mod = sqlite_module();

	if (!mod) return 0;
	return wasm_memory_read(mod, 0, ptr, dst, len) == WASM_OK;
}

static int guest_read_i32(uint32_t ptr, int32_t* out_value) {
	uint8_t bytes[4];
	uint32_t value;

	if (!out_value || !guest_read_bytes(ptr, bytes, sizeof(bytes))) return 0;
	value = (uint32_t)bytes[0] |
			((uint32_t)bytes[1] << 8) |
			((uint32_t)bytes[2] << 16) |
			((uint32_t)bytes[3] << 24);
	*out_value = (int32_t)value;
	return 1;
}

static int guest_write_i32(uint32_t ptr, int32_t value) {
	uint8_t bytes[4];
	uint32_t uvalue = (uint32_t)value;

	bytes[0] = (uint8_t)(uvalue & 0xFFu);
	bytes[1] = (uint8_t)((uvalue >> 8) & 0xFFu);
	bytes[2] = (uint8_t)((uvalue >> 16) & 0xFFu);
	bytes[3] = (uint8_t)((uvalue >> 24) & 0xFFu);
	return guest_write_bytes(ptr, bytes, sizeof(bytes));
}

static int guest_read_string(uint32_t ptr, char* buffer, size_t capacity) {
	wasm_module_t* mod = sqlite_module();

	if (!buffer || capacity == 0u || !mod) return 0;
	return wasm_memory_get_string(mod, 0, ptr, buffer, capacity) == WASM_OK;
}

static int32_t guest_alloc(size_t size) {
	if (size > 0x7fffffffU) return 0;
	return sqlite3_malloc((int32_t)size);
}

static void guest_free(int32_t ptr) {
	if (ptr != 0) sqlite3_free(ptr);
}

static int32_t guest_alloc_string(const char* text) {
	size_t len = text ? strlen(text) : 0u;
	int32_t ptr = guest_alloc(len + 1u);

	if (ptr == 0) return 0;
	if (!guest_write_bytes((uint32_t)ptr, text ? text : "", len + 1u)) {
		guest_free(ptr);
		return 0;
	}
	return ptr;
}

static int copy_db_error(int32_t db_handle, char* buffer, size_t capacity) {
	int32_t message_ptr;

	if (!buffer || capacity == 0u) return 0;
	buffer[0] = '\0';
	if (db_handle == 0) return 0;

	message_ptr = sqlite3_errmsg(db_handle);
	if (message_ptr == 0) return 0;
	return guest_read_string((uint32_t)message_ptr, buffer, capacity);
}

static int exec_sql(int32_t db_handle, const char* sql) {
	int32_t sql_ptr = 0;
	int32_t err_slot = 0;
	int32_t err_ptr = 0;
	int rc;
	char error_text[256];

	sql_ptr = guest_alloc_string(sql);
	err_slot = guest_alloc(4u);
	if (sql_ptr == 0 || err_slot == 0 || !guest_write_i32((uint32_t)err_slot, 0)) {
		fprintf(stderr, "failed to allocate guest memory for SQL statement\n");
		rc = 0;
		goto cleanup;
	}

	rc = sqlite3_exec(db_handle, sql_ptr, 0, 0, err_slot);
	if (rc != SQLITE_OK) {
		if (guest_read_i32((uint32_t)err_slot, &err_ptr) && err_ptr != 0 &&
			guest_read_string((uint32_t)err_ptr, error_text, sizeof(error_text))) {
			fprintf(stderr, "sqlite3_exec failed: %s\n", error_text);
			guest_free(err_ptr);
		} else if (copy_db_error(db_handle, error_text, sizeof(error_text))) {
			fprintf(stderr, "sqlite3_exec failed: %s\n", error_text);
		} else {
			fprintf(stderr, "sqlite3_exec failed with rc=%d\n", rc);
		}
		rc = 0;
		goto cleanup;
	}

	rc = 1;

cleanup:
	guest_free(sql_ptr);
	guest_free(err_slot);
	return rc;
}

static int print_query(int32_t db_handle, const char* title, const char* sql) {
	int32_t sql_ptr = 0;
	int32_t stmt_slot = 0;
	int32_t stmt_handle = 0;
	int rc;
	int column_count;
	int column_index;
	char text_buffer[256];
	char error_text[256];

	sql_ptr = guest_alloc_string(sql);
	stmt_slot = guest_alloc(4u);
	if (sql_ptr == 0 || stmt_slot == 0 || !guest_write_i32((uint32_t)stmt_slot, 0)) {
		fprintf(stderr, "failed to allocate guest memory for query\n");
		rc = 0;
		goto cleanup;
	}

	rc = sqlite3_prepare_v2(db_handle, sql_ptr, -1, stmt_slot, 0);
	guest_free(sql_ptr);
	sql_ptr = 0;
	if (rc != SQLITE_OK) {
		if (copy_db_error(db_handle, error_text, sizeof(error_text)))
			fprintf(stderr, "sqlite3_prepare_v2 failed: %s\n", error_text);
		else
			fprintf(stderr, "sqlite3_prepare_v2 failed with rc=%d\n", rc);
		rc = 0;
		goto cleanup;
	}
	if (!guest_read_i32((uint32_t)stmt_slot, &stmt_handle) || stmt_handle == 0) {
		fprintf(stderr, "sqlite3_prepare_v2 did not return a statement handle\n");
		rc = 0;
		goto cleanup;
	}

	printf("%s\n", title);
	column_count = sqlite3_column_count(stmt_handle);
	for (column_index = 0; column_index < column_count; column_index++) {
		int32_t name_ptr = sqlite3_column_name(stmt_handle, column_index);

		if (column_index != 0) printf(" | ");
		if (name_ptr != 0 && guest_read_string((uint32_t)name_ptr, text_buffer, sizeof(text_buffer)))
			printf("%s", text_buffer);
		else
			printf("column_%d", column_index);
	}
	printf("\n");

	for (;;) {
		rc = sqlite3_step(stmt_handle);
		if (rc == SQLITE_DONE) break;
		if (rc != SQLITE_ROW) {
			if (copy_db_error(db_handle, error_text, sizeof(error_text)))
				fprintf(stderr, "sqlite3_step failed: %s\n", error_text);
			else
				fprintf(stderr, "sqlite3_step failed with rc=%d\n", rc);
			rc = 0;
			goto cleanup;
		}

		for (column_index = 0; column_index < column_count; column_index++) {
			int column_type;

			if (column_index != 0) printf(" | ");
			column_type = sqlite3_column_type(stmt_handle, column_index);
			switch (column_type) {
				case SQLITE_INTEGER:
					printf("%" PRId64, sqlite3_column_int64(stmt_handle, column_index));
					break;
				case SQLITE_FLOAT:
					printf("%.6f", sqlite3_column_double(stmt_handle, column_index));
					break;
				case SQLITE_TEXT: {
					int32_t value_ptr = sqlite3_column_text(stmt_handle, column_index);

					if (value_ptr != 0 && guest_read_string((uint32_t)value_ptr, text_buffer, sizeof(text_buffer)))
						printf("%s", text_buffer);
					else
						printf("<text>");
					break;
				}
				case SQLITE_NULL:
					printf("NULL");
					break;
				default:
					printf("<type=%d>", column_type);
					break;
			}
		}
		printf("\n");
	}

	printf("\n");
	rc = sqlite3_finalize(stmt_handle);
	stmt_handle = 0;
	if (rc != SQLITE_OK) {
		fprintf(stderr, "sqlite3_finalize failed with rc=%d\n", rc);
		rc = 0;
		goto cleanup;
	}

	rc = 1;

cleanup:
	if (stmt_handle != 0) (void)sqlite3_finalize(stmt_handle);
	guest_free(stmt_slot);
	guest_free(sql_ptr);
	return rc;
}

int main(void) {
	uint8_t* wasm_bytes = NULL;
	size_t wasm_len = 0u;
	int32_t db_name_ptr = 0;
	int32_t db_slot = 0;
	int32_t db_handle = 0;
	int32_t version_ptr;
	int rc;
	char version_text[128];
	char error_text[256];
	int exit_code = 1;

	if (!read_file_bytes(SQLITE_WASM_PATH, &wasm_bytes, &wasm_len)) {
		fprintf(stderr, "failed to read %s\n", SQLITE_WASM_PATH);
		return 1;
	}

	rc = sqlite_wasm_init(wasm_bytes, wasm_len, NULL);
	free(wasm_bytes);
	wasm_bytes = NULL;
	if (rc != WASM_OK) {
		report_wrapper_error("sqlite_wasm_init failed");
		goto cleanup;
	}

	rc = sqlite3_initialize();
	if (rc != SQLITE_OK) {
		fprintf(stderr, "sqlite3_initialize failed with rc=%d\n", rc);
		goto cleanup;
	}

	version_ptr = sqlite3_libversion();
	if (version_ptr != 0 && guest_read_string((uint32_t)version_ptr, version_text, sizeof(version_text)))
		printf("sqlite version: %s\n\n", version_text);

	db_name_ptr = guest_alloc_string(":memory:");
	db_slot = guest_alloc(4u);
	if (db_name_ptr == 0 || db_slot == 0 || !guest_write_i32((uint32_t)db_slot, 0)) {
		fprintf(stderr, "failed to allocate guest memory for sqlite3_open\n");
		goto cleanup;
	}

	rc = sqlite3_open(db_name_ptr, db_slot);
	if (!guest_read_i32((uint32_t)db_slot, &db_handle)) {
		fprintf(stderr, "failed to read sqlite3_open output slot\n");
		goto cleanup;
	}
	if (rc != SQLITE_OK) {
		if (copy_db_error(db_handle, error_text, sizeof(error_text)))
			fprintf(stderr, "sqlite3_open failed: %s\n", error_text);
		else
			fprintf(stderr, "sqlite3_open failed with rc=%d\n", rc);
		goto cleanup;
	}

	if (!exec_sql(
			db_handle,
			"create table authors (id integer primary key, name text not null);"
			"create table books (id integer primary key, title text not null, author_id integer not null, year integer not null);"
			"insert into authors values (1, 'Ursula K. Le Guin'), (2, 'Carl Sagan'), (3, 'Neal Stephenson');"
			"insert into books values (1, 'The Dispossessed', 1, 1974),"
			" (2, 'Contact', 2, 1985),"
			" (3, 'Snow Crash', 3, 1992);"))
		goto cleanup;

	if (!print_query(db_handle, "authors", "select id, name from authors order by id;")) goto cleanup;
	if (!print_query(db_handle,
				  "books",
				  "select b.id, b.title, a.name as author, b.year "
				  "from books b join authors a on a.id = b.author_id "
				  "order by b.year desc;"))
		goto cleanup;

	rc = sqlite3_close_v2(db_handle);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "sqlite3_close_v2 failed with rc=%d\n", rc);
		goto cleanup;
	}
	db_handle = 0;

	printf("sqlite_wasm demo passed\n");
	exit_code = 0;

cleanup:
	if (db_handle != 0) (void)sqlite3_close_v2(db_handle);
	guest_free(db_slot);
	guest_free(db_name_ptr);
	sqlite_wasm_free();
	return exit_code;
}