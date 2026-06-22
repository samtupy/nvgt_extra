/* pack.cpp - pack API version 2 implementation
 *
 * NVGT - NonVisual Gaming Toolkit
 * Copyright (c) 2022-2025 Sam Tupy
 * https://nvgt.dev
 * This software is provided "as-is", without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
 * 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
*/

#define NVGT_PLUGIN_INCLUDE
#include "../../src/nvgt_plugin.h"
#include "pack.h"
#include "sqlite3.h"
#include <cstdint>
#include <stdexcept>
#include <Poco/Format.h>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <Poco/StreamUtil.h>
#include <algorithm>
#include <Poco/RegularExpression.h>
#include <type_traits>
#include <array>

using namespace std;

static once_flag SQLITE3MC_INITIALIZER;

void regexp(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
	if (argc != 2) { sqlite3_result_error(ctx, "Expected 2 arguments", -1); return; }
	if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) { sqlite3_result_error(ctx, "Regexp must be a string", -1); return; }
	if (sqlite3_value_type(argv[1]) != SQLITE_TEXT) { sqlite3_result_error(ctx, "String to match must be a string", -1); return; }
	try {
		const string expr(reinterpret_cast<const char*>(sqlite3_value_text(argv[0])));
		const string str(reinterpret_cast<const char*>(sqlite3_value_text(argv[1])));
		Poco::RegularExpression re(expr, Poco::RegularExpression::RE_EXTRA | Poco::RegularExpression::RE_NOTEMPTY | Poco::RegularExpression::RE_UTF8 | Poco::RegularExpression::RE_NO_UTF8_CHECK | Poco::RegularExpression::RE_NEWLINE_ANY);
		Poco::RegularExpression::Match match;
		re.match(str, match);
		sqlite3_result_int(ctx, match.offset == string::npos && match.length == 0 ? 0 : 1);
	} catch (exception& ex) {
		sqlite3_result_error(ctx, ex.what(), -1);
	}
}

// --- Static helpers ---

static string column_string(sqlite3_stmt* stmt, int col) {
	return string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, col)), sqlite3_column_bytes(stmt, col));
}

static sqlite3_stmt* prepare_stmt(sqlite3* db, const char* sql) {
	sqlite3_stmt* stmt;
	if (const auto rc = sqlite3_prepare_v3(db, sql, -1, 0, &stmt, nullptr); rc != SQLITE_OK)
		throw runtime_error(Poco::format("Internal error: %s", string(sqlite3_errmsg(db))));
	return stmt;
}

static void bind_text(sqlite3* db, sqlite3_stmt* stmt, int idx, const string& s) {
	if (const auto rc = sqlite3_bind_text64(stmt, idx, s.data(), s.size(), SQLITE_STATIC, SQLITE_UTF8); rc != SQLITE_OK) {
		sqlite3_finalize(stmt);
		throw runtime_error(Poco::format("Internal error: %s", string(sqlite3_errmsg(db))));
	}
}

template<typename Fn>
static void query_rows(sqlite3* db, sqlite3_stmt* stmt, Fn&& on_row) {
	while (true) {
		const auto rc = sqlite3_step(stmt);
		if (rc == SQLITE_DONE) break;
		if (rc == SQLITE_ROW) { on_row(stmt); continue; }
		if (rc == SQLITE_BUSY) {
			if (!sqlite3_get_autocommit(db)) sqlite3_exec(db, "rollback", nullptr, nullptr, nullptr);
			sqlite3_reset(stmt);
			continue;
		}
		sqlite3_finalize(stmt);
		throw runtime_error(Poco::format("Internal error: %s", string(sqlite3_errmsg(db))));
	}
	sqlite3_finalize(stmt);
}

static void setup_db_write(sqlite3* db, const string& key) {
	sqlite3_db_config(db, SQLITE_DBCONFIG_LOOKASIDE, nullptr, 128, 500);
	if (!key.empty()) {
		if (const auto rc = sqlite3_key_v2(db, "main", key.data(), key.size()); rc != SQLITE_OK)
			throw runtime_error(Poco::format("Internal error: Could not set key: %s", string(sqlite3_errmsg(db))));
	}
	if (const auto rc = sqlite3_exec(db, "pragma journal_mode=wal;", nullptr, nullptr, nullptr); rc != SQLITE_OK)
		throw runtime_error(Poco::format("Internal error: could not set journaling mode: %s", string(sqlite3_errmsg(db))));
	if (const auto rc = sqlite3_exec(db, "create table if not exists pack_files(file_name primary key not null unique, data); create unique index if not exists pack_files_index on pack_files(file_name);", nullptr, nullptr, nullptr); rc != SQLITE_OK)
		throw runtime_error(Poco::format("Internal error: could not create table or index: %s", string(sqlite3_errmsg(db))));
	if (const auto rc = sqlite3_db_config(db, SQLITE_DBCONFIG_DEFENSIVE, 1, NULL); rc != SQLITE_OK)
		throw runtime_error(Poco::format("Internal error: culd not set defensive mode: %s", string(sqlite3_errmsg(db))));
	if (const auto rc = sqlite3_create_function_v2(db, "regexp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_DIRECTONLY, nullptr, &regexp, nullptr, nullptr, nullptr); rc != SQLITE_OK)
		throw runtime_error(Poco::format("Internal error: Could not register regexp function: %s", string(sqlite3_errmsg(db))));
}

static void setup_db_read(sqlite3* db, const string& key) {
	sqlite3_db_config(db, SQLITE_DBCONFIG_LOOKASIDE, nullptr, 128, 500);
	if (!key.empty()) {
		if (const auto rc = sqlite3_key_v2(db, "main", key.data(), key.size()); rc != SQLITE_OK)
			throw runtime_error(Poco::format("Internal error: Could not set key: %s", string(sqlite3_errmsg(db))));
	}
	// mmap maps the file into the virtual address space — reads become memory accesses
	// shared across all connections to the same file via OS page cache
	sqlite3_exec(db, "pragma mmap_size=268435456;", nullptr, nullptr, nullptr);
	sqlite3_exec(db, "pragma cache_size=-4096;", nullptr, nullptr, nullptr);
	if (const auto rc = sqlite3_create_function_v2(db, "regexp", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_DIRECTONLY, nullptr, &regexp, nullptr, nullptr, nullptr); rc != SQLITE_OK)
		throw runtime_error(Poco::format("Internal error: Could not register regexp function: %s", string(sqlite3_errmsg(db))));
}

// Returns the rowid of the newly inserted row
static int64_t insert_blob_from_stream(sqlite3* db, const string& pack_filename, istream& src, uint64_t stream_size) {
	auto stmt = prepare_stmt(db, "insert into pack_files values(?, ?)");
	bind_text(db, stmt, 1, pack_filename);
	if (const auto rc = sqlite3_bind_zeroblob64(stmt, 2, stream_size); rc != SQLITE_OK) {
		sqlite3_finalize(stmt);
		throw runtime_error(Poco::format("Internal error: %s", string(sqlite3_errmsg(db))));
	}
	query_rows(db, stmt, [](sqlite3_stmt*) {});
	const int64_t rowid = sqlite3_last_insert_rowid(db);
	sqlite3_blob* blob;
	if (const auto rc = sqlite3_blob_open(db, "main", "pack_files", "data", rowid, 1, &blob); rc != SQLITE_OK)
		throw runtime_error(Poco::format("Internal error: %s", string(sqlite3_errmsg(db))));
	char buffer[4096];
	int offset = 0;
	while (src) {
		src.read(buffer, 4096);
		const auto n = static_cast<int>(src.gcount());
		if (n == 0) break;
		if (const auto rc = sqlite3_blob_write(blob, buffer, n, offset); rc != SQLITE_OK) {
			sqlite3_blob_close(blob);
			throw runtime_error(Poco::format("Internal error: %s", string(sqlite3_errmsg(db))));
		}
		offset += n;
	}
	sqlite3_blob_close(blob);
	return rowid;
}

// Returns the rowid of the newly inserted row
static int64_t insert_blob_memory(sqlite3* db, const string& pack_filename, const void* data, uint64_t size) {
	auto stmt = prepare_stmt(db, "insert into pack_files values(?, ?)");
	bind_text(db, stmt, 1, pack_filename);
	if (const auto rc = sqlite3_bind_blob64(stmt, 2, data, size, SQLITE_STATIC); rc != SQLITE_OK) {
		sqlite3_finalize(stmt);
		throw runtime_error(Poco::format("Internal error: %s", string(sqlite3_errmsg(db))));
	}
	query_rows(db, stmt, [](sqlite3_stmt*) {});
	return sqlite3_last_insert_rowid(db);
}

// --- pack ---

pack::pack() : db(nullptr), created_from_copy(false), mutable_origin(nullptr) {
	call_once(SQLITE3MC_INITIALIZER, []() {
		sqlite3_initialize();
		CScriptArray::SetMemoryFunctions(std::malloc, std::free);
	});
}

pack::pack(const pack& other) : db(nullptr), created_from_copy(false), mutable_origin(&other) {
	const auto dbptr = other.get_db_ptr();
	const auto filename = sqlite3_filename_database(sqlite3_db_filename(dbptr, "main"));
	if (!filename) throw runtime_error("Cannot create a read-only copy of an in-memory or temporary pack!");
	if (!open(string(filename), SQLITE_OPEN_READONLY, other.get_key())) throw runtime_error("Could not open pack in R/O mode!");
	other.duplicate();
	created_from_copy = true;
}

void pack::load_entry_cache() const {
	entry_cache.clear();
	auto stmt = prepare_stmt(db, "select rowid, file_name, length(data) from pack_files");
	query_rows(db, stmt, [&](sqlite3_stmt* s) {
		const int64_t rowid = sqlite3_column_int64(s, 0);
		string name = column_string(s, 1);
		const uint64_t size = static_cast<uint64_t>(sqlite3_column_int64(s, 2));
		entry_cache.emplace(name, pack_entry{name, size, rowid});
	});
}

bool pack::open(const string& filename, int mode, const string& key) {
	if (mode & SQLITE_OPEN_READONLY) {
		if (const auto rc = sqlite3_open_v2(filename.c_str(), &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_EXRESCODE, nullptr); rc != SQLITE_OK)
			return false;
		setup_db_read(db, key);
	} else {
		if (const auto rc = sqlite3_open_v2(filename.data(), &db, mode | SQLITE_OPEN_EXRESCODE, nullptr); rc != SQLITE_OK)
			return false;
		setup_db_write(db, key);
	}
	if (!key.empty()) set_key(key);
	pack_name = filesystem::canonical(filename).string();
	load_entry_cache();
	return true;
}

bool pack::create(const string& filename, const string& key) {
	return open(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, key);
}

bool pack::open(const string& filename, const string& key, bool rw) {
	return open(filename, rw ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY, key);
}

pack::~pack() {
	if (db && !created_from_copy) {
		sqlite3_close(db);
		db = nullptr;
	}
}

bool pack::rekey(const string& key) {
	if (const auto rc = sqlite3_rekey_v2(db, "main", key.data(), key.size()); rc != SQLITE_OK)
		return false;
	set_key(key);
	return true;
}

bool pack::close() {
	return sqlite3_close(db) == SQLITE_OK;
}

bool pack::add_file(const string& disk_filename, const string& pack_filename, bool allow_replace) {
	if (!filesystem::exists(disk_filename)) return false;
	const auto file_size = filesystem::file_size(disk_filename);
	if (file_size > SQLITE_MAX_LENGTH) return false;
	if (file_exists(pack_filename)) {
		if (allow_replace) delete_file(pack_filename);
		else return false;
	}
	ifstream stream(filesystem::canonical(disk_filename).string(), ios::in | ios::binary);
	const int64_t rowid = insert_blob_from_stream(db, pack_filename, stream, file_size);
	entry_cache.insert_or_assign(pack_filename, pack_entry{pack_filename, file_size, rowid});
	return true;
}

bool pack::add_directory(const string& dir, bool allow_replace) {
	if (!filesystem::exists(dir) || !filesystem::is_directory(dir)) return false;
	if (const auto rc = sqlite3_exec(db, "begin immediate transaction;", nullptr, nullptr, nullptr); rc != SQLITE_OK)
		throw runtime_error(Poco::format("Could not begin transaction: %s", string(sqlite3_errmsg(db))));
	for (const auto& f : filesystem::recursive_directory_iterator(dir)) {
		if (!f.is_regular_file()) continue;
		auto p = f.path().string();
		ranges::replace(p, '\\', '/');
		if (!add_file(f.path().string(), p, allow_replace)) {
			if (!sqlite3_get_autocommit(db)) sqlite3_exec(db, "rollback;", nullptr, nullptr, nullptr);
			return false;
		}
	}
	if (const auto rc = sqlite3_exec(db, "commit;", nullptr, nullptr, nullptr); rc != SQLITE_OK)
		throw runtime_error(Poco::format("Could not commit transaction: %s", string(sqlite3_errmsg(db))));
	return true;
}

bool pack::add_stream(const string& internal_name, void* ds, const bool allow_replace) {
	if (!ds) return false;
	if (file_exists(internal_name)) {
		if (allow_replace) delete_file(internal_name);
		else return false;
	}
	istream* is = dynamic_cast<istream*>(nvgt_datastream_get_ios(ds));
	if (!is) return false;
	is->seekg(0, ios::end);
	const uint64_t stream_size = is->tellg();
	is->seekg(0, ios::beg);
	const int64_t rowid = insert_blob_from_stream(db, internal_name, *is, stream_size);
	entry_cache.insert_or_assign(internal_name, pack_entry{internal_name, stream_size, rowid});
	return true;
}

bool pack::add_memory(const string& pack_filename, unsigned char* data, unsigned int size, bool allow_replace) {
	if (size > SQLITE_MAX_LENGTH) return false;
	if (file_exists(pack_filename)) {
		if (!allow_replace) return false;
		delete_file(pack_filename);
	}
	const int64_t rowid = insert_blob_memory(db, pack_filename, data, size);
	entry_cache.insert_or_assign(pack_filename, pack_entry{pack_filename, size, rowid});
	return true;
}

bool pack::add_memory(const string& pack_filename, const string& data, bool allow_replace) {
	if (data.empty() || data.size() > SQLITE_MAX_LENGTH) return false;
	if (file_exists(pack_filename)) {
		if (!allow_replace) return false;
		delete_file(pack_filename);
	}
	const int64_t rowid = insert_blob_memory(db, pack_filename, data.data(), data.size());
	entry_cache.insert_or_assign(pack_filename, pack_entry{pack_filename, data.size(), rowid});
	return true;
}

bool pack::delete_file(const string& pack_filename) {
	if (!file_exists(pack_filename)) return false;
	auto stmt = prepare_stmt(db, "delete from pack_files where file_name = ?");
	bind_text(db, stmt, 1, pack_filename);
	query_rows(db, stmt, [](sqlite3_stmt*) {});
	entry_cache.erase(pack_filename);
	return true;
}

bool pack::file_exists(const string& pack_filename) {
	return entry_cache.count(pack_filename) > 0;
}

string pack::get_file_name(const int64_t idx) {
	for (const auto& [name, entry] : entry_cache)
		if (entry.rowid == idx) return name;
	return "";
}

void pack::list_files(vector<string>& files) {
	files.reserve(files.size() + entry_cache.size());
	for (const auto& [name, entry] : entry_cache) files.emplace_back(name);
}

int64_t pack::get_file_count() {
	return static_cast<int64_t>(entry_cache.size());
}

CScriptArray* pack::list_files() {
	asIScriptContext* ctx = asGetActiveContext();
	asIScriptEngine* engine = ctx->GetEngine();
	CScriptArray* array = CScriptArray::Create(engine->GetTypeInfoByDecl("array<string>"));
	array->Reserve(static_cast<asUINT>(entry_cache.size()));
	for (const auto& [name, entry] : entry_cache) {
		string n = name;
		array->InsertLast(&n);
	}
	return array;
}

uint64_t pack::get_file_size(const string& pack_filename) {
	auto it = entry_cache.find(pack_filename);
	return it != entry_cache.end() ? it->second.size : 0;
}

int64_t pack::get_rowid(const string& filename) const {
	auto it = entry_cache.find(filename);
	return it != entry_cache.end() ? it->second.rowid : 0;
}

unsigned int pack::read_file(const string& pack_filename, unsigned int offset, unsigned char* buffer, unsigned int size) {
	const auto rowid = get_rowid(pack_filename);
	if (!rowid) return 0;
	sqlite3_blob* blob;
	if (const auto rc = sqlite3_blob_open(db, "main", "pack_files", "data", rowid, 0, &blob); rc != SQLITE_OK)
		throw runtime_error(Poco::format("Internal error: %s", string(sqlite3_errmsg(db))));
	const auto blob_size = static_cast<unsigned>(sqlite3_blob_bytes(blob));
	if (offset >= blob_size || (offset + size) > blob_size) { sqlite3_blob_close(blob); return 0; }
	if (const auto rc = sqlite3_blob_read(blob, buffer, size, offset); rc != SQLITE_OK) {
		sqlite3_blob_close(blob);
		throw runtime_error(Poco::format("Internal error: %s", string(sqlite3_errmsg(db))));
	}
	sqlite3_blob_close(blob);
	return size;
}

string pack::read_file_string(const string& pack_filename, unsigned int offset, unsigned int size) {
	const auto rowid = get_rowid(pack_filename);
	if (!rowid) return "";
	sqlite3_blob* blob;
	if (const auto rc = sqlite3_blob_open(db, "main", "pack_files", "data", rowid, 0, &blob); rc != SQLITE_OK)
		throw runtime_error(Poco::format("Internal error: %s", string(sqlite3_errmsg(db))));
	const auto blob_size = static_cast<unsigned>(sqlite3_blob_bytes(blob));
	if (offset >= blob_size || (offset + size) > blob_size) { sqlite3_blob_close(blob); return ""; }
	string res(size, '\0');
	if (const auto rc = sqlite3_blob_read(blob, res.data(), size, offset); rc != SQLITE_OK) {
		sqlite3_blob_close(blob);
		throw runtime_error(Poco::format("Internal error: %s", string(sqlite3_errmsg(db))));
	}
	sqlite3_blob_close(blob);
	return res;
}

uint64_t pack::size() {
	uint64_t total = 0;
	for (const auto& [_, entry] : entry_cache) total += entry.size;
	return total;
}

blob_stream pack::open_file_stream(const string& file_name, const bool rw) {
	const auto rowid = get_rowid(file_name);
	if (!rowid) throw ios_base::failure(Poco::format("File %s does not exist", file_name));
	return blob_stream(db, "main", "pack_files", "data", rowid, rw);
}

void pack::allocate_file(const string& file_name, const int64_t size, const bool allow_replace) {
	if (file_exists(file_name)) {
		if (allow_replace) delete_file(file_name);
		else throw runtime_error(Poco::format("Could not allocate file %s because it already exists", file_name));
	}
	auto stmt = prepare_stmt(db, "insert into pack_files values(?, ?)");
	bind_text(db, stmt, 1, file_name);
	if (const auto rc = sqlite3_bind_zeroblob64(stmt, 2, size); rc != SQLITE_OK) {
		sqlite3_finalize(stmt);
		throw runtime_error(Poco::format("Internal error: %s", string(sqlite3_errmsg(db))));
	}
	query_rows(db, stmt, [](sqlite3_stmt*) {});
	const int64_t rowid = sqlite3_last_insert_rowid(db);
	entry_cache.emplace(file_name, pack_entry{file_name, static_cast<uint64_t>(size), rowid});
}

bool pack::rename_file(const string& old, const string& new_) {
	if (!file_exists(old)) return false;
	auto stmt = prepare_stmt(db, "update pack_files set file_name = ? where file_name = ?");
	bind_text(db, stmt, 1, new_);
	bind_text(db, stmt, 2, old);
	query_rows(db, stmt, [](sqlite3_stmt*) {});
	auto node = entry_cache.extract(old);
	if (!node.empty()) { node.key() = new_; node.mapped().name = new_; entry_cache.insert(std::move(node)); }
	return true;
}

void pack::clear() {
	auto stmt = prepare_stmt(db, "delete from pack_files");
	query_rows(db, stmt, [](sqlite3_stmt*) {});
	entry_cache.clear();
}

CScriptArray* pack::find(const string& what, const FindMode mode) {
	asIScriptContext* ctx = asGetActiveContext();
	asIScriptEngine* engine = ctx->GetEngine();
	CScriptArray* array = CScriptArray::Create(engine->GetTypeInfoByDecl("array<string>"));
	static const char* queries[] = {
		"select file_name from pack_files where file_name like ?",
		"select file_name from pack_files where file_name glob ?",
		"select file_name from pack_files where file_name regexp ?"
	};
	auto stmt = prepare_stmt(db, queries[static_cast<int>(mode)]);
	bind_text(db, stmt, 1, what);
	query_rows(db, stmt, [&](sqlite3_stmt* s) {
		string res = column_string(s, 0);
		array->InsertLast(&res);
	});
	return array;
}

CScriptArray* pack::exec(const string& sql) {
	asIScriptContext* ctx = asGetActiveContext();
	asIScriptEngine* engine = ctx->GetEngine();
	CScriptArray* array = CScriptArray::Create(engine->GetTypeInfoByDecl("array<dictionary@>"));
	char* errmsg;
	if (const auto rc = sqlite3_exec(db, sql.data(), [](void* arr, int column_count, char** column_data, char** columns) -> int {
		CScriptArray* array = (CScriptArray*)arr;
		asIScriptContext* ctx = asGetActiveContext();
		asIScriptEngine* engine = ctx->GetEngine();
		asITypeInfo* string_type = engine->GetTypeInfoByDecl("string");
		CScriptDictionary* d = CScriptDictionary::Create(engine);
		for (auto i = 0; i < column_count; ++i) {
			if (column_data[i]) d->Set(columns[i], column_data[i], string_type->GetTypeId());
			else { string null_string = "NULL"; d->Set(columns[i], &null_string, string_type->GetTypeId()); }
		}
		array->InsertLast(&d);
		return 0;
	}, array, &errmsg); rc != SQLITE_OK) {
		if (errmsg) {
			string error(errmsg);
			sqlite3_free(errmsg);
			throw runtime_error(error);
		} else throw runtime_error("Unknown error");
	}
	return array;
}

void* pack::open_file(const string& file_name, const bool rw) {
	return nvgt_datastream_create(new blob_stream(open_file_stream(file_name, rw)), "", 1);
}

istream* pack::get_file(const string& filename) const {
	try {
		return new blob_stream(const_cast<pack*>(this)->open_file_stream(filename, false));
	} catch (exception&) {
		return nullptr;
	}
}

sqlite3* pack::get_db_ptr() const {
	if (!db) throw runtime_error("DB pointer is null!");
	return db;
}

void pack::set_db_ptr(sqlite3* ptr) {
	if (!ptr) throw runtime_error("db pointer is null!");
	db = ptr;
}

const string pack::get_pack_name() const {
	return pack_name;
}

bool pack::extract_file(const string& internal_name, const string& file_on_disk) {
	const auto rowid = get_rowid(internal_name);
	if (!rowid) return false;
	sqlite3_blob* blob;
	if (const auto rc = sqlite3_blob_open(db, "main", "pack_files", "data", rowid, 0, &blob); rc != SQLITE_OK)
		throw runtime_error(string(sqlite3_errmsg(db)));
	ofstream stream(file_on_disk, ios::out | ios::binary);
	if (!stream) { sqlite3_blob_close(blob); return false; }
	array<char, 4096> buffer;
	int offset = 0;
	const int blob_size = sqlite3_blob_bytes(blob);
	while (offset < blob_size) {
		const int to_read = min(static_cast<int>(buffer.size()), blob_size - offset);
		if (const auto rc = sqlite3_blob_read(blob, buffer.data(), to_read, offset); rc != SQLITE_OK) {
			sqlite3_blob_close(blob);
			throw runtime_error(string(sqlite3_errmsg(db)));
		}
		stream.write(buffer.data(), to_read);
		offset += to_read;
	}
	sqlite3_blob_close(blob);
	return !stream.bad() && !stream.fail();
}

void pack::set_key(const string& key) { pack_key = key; }
string pack::get_key() const { return pack_key; }

sqlite3statement* pack::prepare(const string& statement, const bool persistant) {
	sqlite3_stmt* stmt;
	if (const auto rc = sqlite3_prepare_v3(db, statement.data(), statement.size(), persistant ? SQLITE_PREPARE_PERSISTENT : 0, &stmt, nullptr); rc != SQLITE_OK)
		throw runtime_error(Poco::format("Parse error: %s", string(sqlite3_errmsg(db))));
	return new sqlite3statement(stmt);
}

const pack_interface* pack::make_immutable() const { return new pack(*this); }

const pack_interface* pack::get_mutable() const {
	if (mutable_origin) { mutable_origin->duplicate(); return mutable_origin; }
	this->duplicate();
	return this;
}

// --- blob_stream_buf ---

blob_stream_buf::blob_stream_buf(bool read_write) : Poco::BufferedBidirectionalStreamBuf(8192, read_write ? ios::in | ios::out : ios::in), read_pos(0), write_pos(0), blob(nullptr), blob_size(0) {}

blob_stream_buf::~blob_stream_buf() {
	if (blob) {
		sqlite3_blob_close(blob);
		blob = nullptr;
	}
}

void blob_stream_buf::open(sqlite3* s, const string_view& db, const string_view& table, const string_view& column, const sqlite3_int64 row, const bool read_write) {
	if (const auto rc = sqlite3_blob_open(s, db.data(), table.data(), column.data(), row, static_cast<int>(read_write), &blob); rc != SQLITE_OK)
		throw runtime_error(Poco::format("%s", string(sqlite3_errmsg(s))));
	blob_size = sqlite3_blob_bytes(blob);
}

blob_stream_buf::pos_type blob_stream_buf::seekoff(off_type off, ios_base::seekdir dir, ios_base::openmode which) {
	if ((which & ios_base::in) != 0 && (which & ios_base::out) != 0)
		return pos_type(-1);
	if ((which & ios_base::out) != 0)
		if (sync() == -1) return pos_type(-1);
	off_type current_pos;
	if ((which & ios_base::in) != 0)
		current_pos = read_pos - pos_type(egptr() - gptr());
	else if ((which & ios_base::out) != 0)
		current_pos = write_pos;
	else
		return pos_type(-1);
	off_type new_pos;
	switch (dir) {
		case ios_base::beg: new_pos = off; break;
		case ios_base::cur: new_pos = current_pos + off; break;
		case ios_base::end: new_pos = static_cast<off_type>(blob_size) + off; break;
		default: return pos_type(-1);
	}
	if (new_pos < 0 || new_pos > blob_size) return pos_type(-1);
	if ((which & ios_base::in) != 0) { setg(nullptr, nullptr, nullptr); read_pos = new_pos; }
	else { setp(nullptr, nullptr); write_pos = new_pos; }
	return pos_type(new_pos);
}

blob_stream_buf::pos_type blob_stream_buf::seekpos(pos_type pos, ios_base::openmode which) {
	return seekoff(off_type(pos), ios_base::beg, which);
}

int blob_stream_buf::readFromDevice(char_type* buffer, streamsize length) {
	if (read_pos >= blob_size || read_pos < 0) return char_traits::eof();
	const auto len = min(length, static_cast<streamsize>(blob_size - read_pos));
	if (const auto rc = sqlite3_blob_read(blob, buffer, len, read_pos); rc != SQLITE_OK)
		throw runtime_error(sqlite3_errstr(rc));
	read_pos += len;
	return len;
}

int blob_stream_buf::writeToDevice(const char_type* buffer, streamsize length) {
	if (write_pos >= blob_size) return char_traits::eof();
	const auto len = min(length, static_cast<streamsize>(blob_size - write_pos));
	if (const auto rc = sqlite3_blob_write(blob, buffer, len, write_pos); rc != SQLITE_OK)
		throw runtime_error(sqlite3_errstr(rc));
	write_pos += len;
	return len;
}

// --- blob_ios / blob_stream ---

blob_ios::blob_ios(bool read_write) : _buf(read_write) { poco_ios_init(&_buf); }

void blob_ios::open(sqlite3* s, const string_view& db, const string_view& table, const string_view& column, const sqlite3_int64 row, const bool read_write) {
	_buf.open(s, db, table, column, row, read_write);
}

blob_stream_buf* blob_ios::rdbuf() { return &_buf; }

blob_stream::blob_stream() : blob_ios(false), iostream(&_buf) {}

blob_stream::blob_stream(sqlite3* s, const string_view& db, const string_view& table, const string_view& column, const sqlite3_int64 row, const bool read_write) : blob_ios(read_write), iostream(&_buf) {
	open(s, db, table, column, row, read_write);
}

// --- AngelScript registration ---

pack* ScriptPack_Factory() { return new pack(); }

void RegisterScriptPack(asIScriptEngine* engine) {
	engine->RegisterEnum("pack_open_mode");
	engine->RegisterEnumValue("pack_open_mode", "SQLITE_PACK_OPEN_MODE_READ_ONLY", SQLITE_OPEN_READONLY);
	engine->RegisterEnumValue("pack_open_mode", "SQLITE_PACK_OPEN_MODE_READ_WRITE", SQLITE_OPEN_READWRITE);
	engine->RegisterEnumValue("pack_open_mode", "SQLITE_PACK_OPEN_MODE_CREATE", SQLITE_OPEN_CREATE);
	engine->RegisterEnumValue("pack_open_mode", "SQLITE_PACK_OPEN_MODE_URI", SQLITE_OPEN_URI);
	engine->RegisterEnumValue("pack_open_mode", "SQLITE_PACK_OPEN_MODE_MEMORY", SQLITE_OPEN_MEMORY);
	engine->RegisterEnumValue("pack_open_mode", "SQLITE_PACK_OPEN_MODE_NO_MUTEX", SQLITE_OPEN_NOMUTEX);
	engine->RegisterEnumValue("pack_open_mode", "SQLITE_PACK_OPEN_MODE_FULL_MUTEX", SQLITE_OPEN_FULLMUTEX);
	engine->RegisterEnumValue("pack_open_mode", "SQLITE_PACK_OPEN_MODE_SHARED_CACHE", SQLITE_OPEN_SHAREDCACHE);
	engine->RegisterEnumValue("pack_open_mode", "SQLITE_PACK_OPEN_MODE_PRIVATE_CACHE", SQLITE_OPEN_PRIVATECACHE);
	engine->RegisterEnumValue("pack_open_mode", "SQLITE_PACK_OPEN_MODE_NO_FOLLOW", SQLITE_OPEN_NOFOLLOW);
	engine->RegisterEnum("sqlite_pack_find_mode");
	engine->RegisterEnumValue("sqlite_pack_find_mode", "SQLITE_PACK_FIND_MODE_LIKE", static_cast<underlying_type_t<FindMode>>(FindMode::Like));
	engine->RegisterEnumValue("sqlite_pack_find_mode", "SQLITE_PACK_FIND_MODE_GLOB", static_cast<underlying_type_t<FindMode>>(FindMode::Glob));
	engine->RegisterEnumValue("sqlite_pack_find_mode", "SQLITE_PACK_FIND_MODE_REGEXP", static_cast<underlying_type_t<FindMode>>(FindMode::Regexp));
	engine->RegisterObjectType("sqlite_pack", 0, asOBJ_REF);
	engine->RegisterObjectBehaviour("sqlite_pack", asBEHAVE_FACTORY, "sqlite_pack @p()", asFUNCTION(ScriptPack_Factory), asCALL_CDECL);
	engine->RegisterObjectBehaviour("sqlite_pack", asBEHAVE_ADDREF, "void f()", asMETHOD(pack, duplicate), asCALL_THISCALL);
	engine->RegisterObjectBehaviour("sqlite_pack", asBEHAVE_RELEASE, "void f()", asMETHOD(pack, release), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool open(const string &in filename, const int mode = SQLITE_PACK_OPEN_MODE_READ_ONLY, const string& key = \"\")", asMETHODPR(pack, open, (const string&, int, const string&), bool), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool rekey(const string& key)", asMETHOD(pack, rekey), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool close()", asMETHOD(pack, close), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool add_file(const string &in disc_filename, const string& in pack_filename, bool allow_replace = false)", asMETHOD(pack, add_file), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool add_directory(const string &in dir, const bool allow_replace = false)", asMETHOD(pack, add_directory), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool add_memory(const string &in pack_filename, const string& in data, bool allow_replace = false)", asMETHODPR(pack, add_memory, (const string&, const string&, bool), bool), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool delete_file(const string &in pack_filename)", asMETHOD(pack, delete_file), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool file_exists(const string &in pack_filename) const", asMETHOD(pack, file_exists), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "string get_file_name(int64 index) const", asMETHODPR(pack, get_file_name, (int64_t), string), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "string[]@ list_files() const", asMETHODPR(pack, list_files, (), CScriptArray*), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "uint get_file_size(const string &in pack_filename) const", asMETHOD(pack, get_file_size), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "string read_file(const string &in pack_filename, uint offset_in_file, uint read_byte_count) const", asMETHOD(pack, read_file_string), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool get_active() const property", asMETHOD(pack, get_is_active), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "uint get_size() const property", asMETHOD(pack, size), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "datastream@ get_file(const string&in file_name, const bool rw = false)", asMETHODPR(pack, open_file, (const string&, const bool), void*), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "void allocate_file(const string& file_name, const int64 size, const bool allow_replace = false)", asMETHODPR(pack, allocate_file, (const string&, const int64_t, const bool), void), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool rename_file(const string& old, const string& new_)", asMETHOD(pack, rename_file), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "void clear()", asMETHOD(pack, clear), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "sqlite3statement@ prepare(const string& statement, const bool persistant = false)", asMETHOD(pack, prepare), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "string[]@ find(const string& what, const sqlite_pack_find_mode mode = SQLITE_PACK_FIND_MODE_LIKE)", asMETHOD(pack, find), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "dictionary@[]@ exec(const string& sql)", asMETHOD(pack, exec), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "pack_interface@ opImplCast()", asFUNCTION((pack_interface::op_cast<pack, pack_interface>)), asCALL_CDECL_OBJFIRST);
	engine->RegisterObjectMethod("pack_interface", "sqlite_pack@ opCast()", asFUNCTION((pack_interface::op_cast<pack_interface, pack>)), asCALL_CDECL_OBJFIRST);
	engine->RegisterObjectMethod("sqlite_pack", "bool create(const string &in filename, const string&in key = \"\")", asMETHOD(pack, create), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool open(const string &in filename, const string &in key = \"\", const bool rw = false)", asMETHODPR(pack, open, (const string&, const string&, bool), bool), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool add_stream(const string &in internal_name, datastream@ ds, const bool allow_replace=false)", asMETHOD(pack, add_stream), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "int64 get_file_count() const property", asMETHOD(pack, get_file_count), asCALL_THISCALL);
	engine->RegisterObjectMethod("sqlite_pack", "bool extract_file(const string &in internal_name, const string &in file_on_disk)", asMETHOD(pack, extract_file), asCALL_THISCALL);
}
