#include <filesystem>
#include <chrono>
#include <cstdint>
#include <llfio.hpp>
#include <span>
#include <string>
#include <vector>
#include <scn/scan.h>
#include <cstdint>
#include "math/simd.h"
#include "nvgt_plugin.h"

namespace llfio = LLFIO_V2_NAMESPACE;

std::string read_file(const std::string& file_name, llfio::file_handle::mode mode,
                      llfio::file_handle::creation creation, llfio::file_handle::caching caching,
                      llfio::file_handle::flag flags) {
  const auto absolute_path { std::filesystem::absolute(file_name) };
  llfio::file_handle fh{llfio::file({}, absolute_path, mode, creation, caching, flags).value()};
  std::string buffer {};
  buffer.resize(fh.maximum_extent().value());
  auto buffer_view{std::as_writable_bytes(std::span{buffer})};
  const auto bytesread{
      llfio::read(fh, 0, {{buffer_view.data(), buffer_view.size()}}, std::chrono::minutes(1))
          .value()};
  if (bytesread == 0) {
    return "";
  }
  buffer.resize(bytesread);
  fh.close().value();
  return buffer;
}

void write_file(const std::string& file_name, const std::string& contents,
                llfio::file_handle::mode mode, llfio::file_handle::creation creation,
                llfio::file_handle::caching caching, llfio::file_handle::flag flags) {
  const auto absolute_path{std::filesystem::absolute(file_name)};
  llfio::file_handle fh{llfio::file({}, absolute_path, mode, creation, caching, flags).value()};
  fh.truncate(0).value();
  const auto contents_view{std::as_bytes(std::span{contents})};
  std::size_t offset{0};
  while (offset < contents_view.size()) {
    const auto bytes_transferred{
        fh.write(offset, {{contents_view.data(), contents_view.size()}}, std::chrono::minutes(1))
            .value()};
    if (bytes_transferred == 0) {
      throw std::runtime_error("Encountered null write!");
    }
    offset += bytes_transferred;
  }
  fh.close().value();
}

// int8
std::int8_t parse_int8(const std::string& num, const int base) {
    return scn::scan_int<std::int8_t>(num, base).value().value();
}

// uint8
std::uint8_t parse_uint8(const std::string& num, const int base) {
    return scn::scan_int<std::uint8_t>(num, base).value().value();
}

// int16
std::int16_t parse_int16(const std::string& num, const int base) {
    return scn::scan_int<std::int16_t>(num, base).value().value();
}

// uint16
std::uint16_t parse_uint16(const std::string& num, const int base) {
    return scn::scan_int<std::uint16_t>(num, base).value().value();
}

// int32
std::int32_t parse_int32(const std::string& num, const int base) {
    return scn::scan_int<std::int32_t>(num, base).value().value();
}

// uint32
std::uint32_t parse_uint32(const std::string& num, const int base) {
    return scn::scan_int<std::uint32_t>(num, base).value().value();
}

// int64
std::int64_t parse_int64(const std::string& num, const int base) {
    return scn::scan_int<std::int64_t>(num, base).value().value();
}

// uint64
std::uint64_t parse_uint64(const std::string& num, const int base) {
    return scn::scan_int<std::uint64_t>(num, base).value().value();
}

// int8
std::int8_t parse_int8_fast(const std::string& num) {
    return scn::scan_int_exhaustive_valid<std::int8_t>(num);
}

// uint8
std::uint8_t parse_uint8_fast(const std::string& num) {
    return scn::scan_int_exhaustive_valid<std::uint8_t>(num);
}

// int16
std::int16_t parse_int16_fast(const std::string& num) {
    return scn::scan_int_exhaustive_valid<std::int16_t>(num);
}

// uint16
std::uint16_t parse_uint16_fast(const std::string& num) {
    return scn::scan_int_exhaustive_valid<std::uint16_t>(num);
}

// int32
std::int32_t parse_int32_fast(const std::string& num) {
    return scn::scan_int_exhaustive_valid<std::int32_t>(num);
}

// uint32
std::uint32_t parse_uint32_fast(const std::string& num) {
    return scn::scan_int_exhaustive_valid<std::uint32_t>(num);
}

// int64
std::int64_t parse_int64_fast(const std::string& num) {
    return scn::scan_int_exhaustive_valid<std::int64_t>(num);
}

// uint64
std::uint64_t parse_uint64_fast(const std::string& num) {
    return scn::scan_int_exhaustive_valid<std::uint64_t>(num);
}

plugin_main(nvgt_plugin_shared* shared) {
  prepare_plugin(shared);
  register_simd_elementary_functions(shared->script_engine);
  shared->script_engine->SetDefaultNamespace("fast");
  shared->script_engine->RegisterEnum("mode");
  shared->script_engine->RegisterEnumValue("mode", "mode_unchanged",
                                           static_cast<int>(llfio::file_handle::mode::unchanged));
  shared->script_engine->RegisterEnumValue("mode", "mode_none",
                                           static_cast<int>(llfio::file_handle::mode::none));
  shared->script_engine->RegisterEnumValue("mode", "mode_attr_read",
                                           static_cast<int>(llfio::file_handle::mode::attr_read));
  shared->script_engine->RegisterEnumValue("mode", "mode_attr_write",
                                           static_cast<int>(llfio::file_handle::mode::attr_write));
  shared->script_engine->RegisterEnumValue("mode", "mode_read",
                                           static_cast<int>(llfio::file_handle::mode::read));
  shared->script_engine->RegisterEnumValue("mode", "mode_write",
                                           static_cast<int>(llfio::file_handle::mode::write));
  shared->script_engine->RegisterEnumValue("mode", "mode_append",
                                           static_cast<int>(llfio::file_handle::mode::append));
  shared->script_engine->RegisterEnum("creation");
  shared->script_engine->RegisterEnumValue(
      "creation", "creation_open_existing",
      static_cast<int>(llfio::file_handle::creation::open_existing));
  shared->script_engine->RegisterEnumValue(
      "creation", "creation_only_if_not_exist",
      static_cast<int>(llfio::file_handle::creation::only_if_not_exist));
  shared->script_engine->RegisterEnumValue(
      "creation", "creation_if_needed", static_cast<int>(llfio::file_handle::creation::if_needed));
  shared->script_engine->RegisterEnumValue(
      "creation", "creation_truncate_existing",
      static_cast<int>(llfio::file_handle::creation::truncate_existing));
  shared->script_engine->RegisterEnumValue(
      "creation", "creation_always_new",
      static_cast<int>(llfio::file_handle::creation::always_new));
  shared->script_engine->RegisterEnum("caching");
  shared->script_engine->RegisterEnumValue(
      "caching", "caching_unchanged", static_cast<int>(llfio::file_handle::caching::unchanged));
  shared->script_engine->RegisterEnumValue("caching", "caching_none",
                                           static_cast<int>(llfio::file_handle::caching::none));
  shared->script_engine->RegisterEnumValue(
      "caching", "caching_only_metadata",
      static_cast<int>(llfio::file_handle::caching::only_metadata));
  shared->script_engine->RegisterEnumValue("caching", "caching_reads",
                                           static_cast<int>(llfio::file_handle::caching::reads));
  shared->script_engine->RegisterEnumValue(
      "caching", "caching_reads_and_metadata",
      static_cast<int>(llfio::file_handle::caching::reads_and_metadata));
  shared->script_engine->RegisterEnumValue("caching", "caching_all",
                                           static_cast<int>(llfio::file_handle::caching::all));
  shared->script_engine->RegisterEnumValue(
      "caching", "caching_safety_barriers",
      static_cast<int>(llfio::file_handle::caching::safety_barriers));
  shared->script_engine->RegisterEnumValue(
      "caching", "caching_temporary", static_cast<int>(llfio::file_handle::caching::temporary));
  shared->script_engine->RegisterEnum("flag");
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_none", static_cast<std::uint16_t>(llfio::file_handle::flag::none));
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_unlink_on_first_close",
      static_cast<std::uint16_t>(llfio::file_handle::flag::unlink_on_first_close));
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_disable_safety_barriers",
      static_cast<std::uint16_t>(llfio::file_handle::flag::disable_safety_barriers));
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_disable_safety_unlinks",
      static_cast<std::uint16_t>(llfio::file_handle::flag::disable_safety_unlinks));
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_disable_prefetching",
      static_cast<std::uint16_t>(llfio::file_handle::flag::disable_prefetching));
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_maximum_prefetching",
      static_cast<std::uint16_t>(llfio::file_handle::flag::maximum_prefetching));
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_win_disable_unlink_emulation",
      static_cast<std::uint16_t>(llfio::file_handle::flag::win_disable_unlink_emulation));
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_win_disable_sparse_file_creation",
      static_cast<std::uint16_t>(llfio::file_handle::flag::win_disable_sparse_file_creation));
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_disable_parallelism",
      static_cast<std::uint16_t>(llfio::file_handle::flag::disable_parallelism));
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_win_create_case_sensitive_directory",
      static_cast<std::uint16_t>(llfio::file_handle::flag::win_create_case_sensitive_directory));
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_multiplexable",
      static_cast<std::uint16_t>(llfio::file_handle::flag::multiplexable));
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_byte_lock_insanity",
      static_cast<std::uint16_t>(llfio::file_handle::flag::byte_lock_insanity));
  shared->script_engine->RegisterEnumValue(
      "flag", "flag_anonymous_inode",
      static_cast<std::uint16_t>(llfio::file_handle::flag::anonymous_inode));
  shared->script_engine->RegisterGlobalFunction(
      "string read_file(const string &in, mode = mode_read, creation = creation_open_existing, "
      "caching = caching_all, flag = flag_multiplexable)",
      asFUNCTION(read_file), asCALL_CDECL);
  shared->script_engine->RegisterGlobalFunction(
      "void write_file(const string &in, const string &in, mode = mode_write, creation = "
      "creation_if_needed, caching = caching_all, flag = flag_multiplexable)",
      asFUNCTION(write_file), asCALL_CDECL);
// Integer parsing routines
// int8
shared->script_engine->RegisterGlobalFunction("int8 parse_int8(const string&, const int)", asFUNCTION(parse_int8), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("int8 parse_int8(const string&)", asFUNCTION(parse_int8_fast), asCALL_CDECL);

// uint8
shared->script_engine->RegisterGlobalFunction("uint8 parse_uint8(const string&, const int)", asFUNCTION(parse_uint8), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("uint8 parse_uint8(const string&)", asFUNCTION(parse_uint8_fast), asCALL_CDECL);

// int16
shared->script_engine->RegisterGlobalFunction("int16 parse_int16(const string&, const int)", asFUNCTION(parse_int16), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("int16 parse_int16(const string&)", asFUNCTION(parse_int16_fast), asCALL_CDECL);

// uint16
shared->script_engine->RegisterGlobalFunction("uint16 parse_uint16(const string&, const int)", asFUNCTION(parse_uint16), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("uint16 parse_uint16(const string&)", asFUNCTION(parse_uint16_fast), asCALL_CDECL);

// int32
shared->script_engine->RegisterGlobalFunction("int32 parse_int32(const string&, const int)", asFUNCTION(parse_int32), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("int32 parse_int32(const string&)", asFUNCTION(parse_int32_fast), asCALL_CDECL);

// uint32
shared->script_engine->RegisterGlobalFunction("uint32 parse_uint32(const string&, const int)", asFUNCTION(parse_uint32), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("uint32 parse_uint32(const string&)", asFUNCTION(parse_uint32_fast), asCALL_CDECL);

// int64
shared->script_engine->RegisterGlobalFunction("int64 parse_int64(const string&, const int)", asFUNCTION(parse_int64), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("int64 parse_int64(const string&)", asFUNCTION(parse_int64_fast), asCALL_CDECL);

// uint64
shared->script_engine->RegisterGlobalFunction("uint64 parse_uint64(const string&, const int)", asFUNCTION(parse_uint64), asCALL_CDECL);
shared->script_engine->RegisterGlobalFunction("uint64 parse_uint64(const string&)", asFUNCTION(parse_uint64_fast), asCALL_CDECL);
  shared->script_engine->SetDefaultNamespace("");
  return true;
}
