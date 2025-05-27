/* pack.h - pack API version 2 implementation header
 *
 * NVGT - NonVisual Gaming Toolkit
 * Copyright (c) 2022-2024 Sam Tupy
 * https://nvgt.gg
 * This software is provided "as-is", without any express or implied warranty. In no event will the authors be held liable for any damages arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose, including commercial applications, and to alter it and redistribute it freely, subject to the following restrictions:
 * 1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
*/

#pragma once

#include "sqlite3.h"
#include <cstdint>
#include <unordered_map>
#include <Poco/RefCountedObject.h>
#include <scriptarray.h>
#include <scriptdictionary.h>
#include <vector>
#include <Poco/BufferedBidirectionalStreamBuf.h>
#include <string_view>
#include <iostream>
#include <ios>
#include "nvgt_sqlite.h"
#include <memory>

enum class FindMode {
	Like,
	Glob,
	Regexp
};

class blob_stream;

class pack : public pack_interface {
private:
	sqlite3* db;
	bool created_from_copy;
	const pack* mutable_origin;
	std::string pack_name;
	std::string pack_key;
public:
	pack();
	pack(const pack& other);
	~pack();
	bool open(const std::string& filename, int mode, const std::string& key);
	bool create(const std::string& filename, const std::string& key);
	bool open(const std::string& filename, const std::string& key);
	bool rekey(const std::string& key);
	bool close();
	bool add_file(const std::string& disk_filename, const std::string& pack_filename, bool allow_replace = false);
	bool add_directory(const std::string& dir, bool allow_replace = false);
	bool add_memory(const std::string& pack_filename, unsigned char* data, unsigned int size, bool allow_replace = false);
	bool add_memory(const std::string& pack_filename, const std::string& data, bool allow_replace = false);
	bool add_stream(const std::string &internal_name, void* ds, const bool allow_replace);
	bool delete_file(const std::string& pack_filename);
	bool file_exists(const std::string& pack_filename);
	std::string get_file_name(std::int64_t idx);
	void list_files(std::vector<std::string>& files);
	CScriptArray* list_files();
	std::uint64_t get_file_size(const std::string& pack_filename);
	unsigned int read_file(const std::string& pack_filename, unsigned int offset, unsigned char* buffer, unsigned int size);
	std::string read_file_string(const std::string& pack_filename, unsigned int offset, unsigned int size);
	std::uint64_t size();
	int64_t get_file_count();
	bool is_active() {
		return db;
	}
	blob_stream open_file_stream(const std::string& file_name, const bool rw);
	void* open_file(const std::string& file_name, const bool rw);
	void allocate_file(const std::string& file_name, const std::int64_t size, const bool allow_replace = false);
	bool rename_file(const std::string& old, const std::string& new_);
	void clear();
	sqlite3statement* prepare(const std::string& statement, const bool persistant = false);
	CScriptArray* find(const std::string& what, const FindMode mode = FindMode::Like);
	CScriptArray* exec(const std::string& sql);
	std::istream* get_file(const std::string& filename) const override;
	sqlite3* get_db_ptr() const;
	void set_db_ptr(sqlite3* ptr);
	const pack_interface* make_immutable() const override;
	const pack_interface* get_mutable() const override;
	const std::string get_pack_name() const override;
	bool extract_file(const std::string &internal_name, const std::string &file_on_disk);
	void set_key(const std::string& key);
	std::string get_key() const;
};

class blob_stream_buf: public Poco::BufferedBidirectionalStreamBuf {
	using pos_type = std::basic_streambuf<char, std::char_traits<char>>::pos_type;
public:
	blob_stream_buf();
	~blob_stream_buf();
	void open(sqlite3* s, const std::string_view& db, const std::string_view& table, const std::string_view& column, const sqlite3_int64 row, const bool read_write);
protected:
	pos_type seekoff( off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out ) override;
	pos_type seekpos(pos_type pos, std::ios_base::openmode which = std::ios_base::in | std::ios_base::out ) override;
private:
	pos_type read_pos, write_pos;
	int readFromDevice(char_type* buffer, std::streamsize length) override;
	int writeToDevice(const char_type* buffer, std::streamsize length) override;
	sqlite3_blob* blob;
};

class blob_ios: public virtual std::ios {
public:
	blob_ios();
	void open(sqlite3* s, const std::string_view& db, const std::string_view& table, const std::string_view& column, const sqlite3_int64 row, const bool read_write);
	blob_stream_buf* rdbuf();
protected:
	blob_stream_buf _buf;
};

class blob_stream: public blob_ios, public std::iostream {
public:
	blob_stream();
	blob_stream(sqlite3* s, const std::string_view& db, const std::string_view& table, const std::string_view& column, const sqlite3_int64 row, const bool read_write);
};

void RegisterScriptPack(asIScriptEngine* engine);
