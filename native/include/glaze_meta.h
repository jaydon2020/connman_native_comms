// Copyright (c) Joel Winarske. All rights reserved.
// Source: https://github.com/jwinarske/bluez_native_comms
//
// glaze_meta.h — lightweight compile-time struct reflection for native_comms
// Channel B payloads. Provides glz::meta<T> and glz::field() used by
// connman_types.h to describe struct fields for binary serialization.
//
// Vendored from native_comms and extended with encode/decode overloads for
// ConnMan-specific types (int16_t, uint16_t, vector<uint8_t>, map, nested
// structs).

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace glz {

// Field descriptor: a name + member pointer pair.
template <typename T, typename MemberPtr>
struct FieldDescriptor {
  const char* name;
  MemberPtr pointer;
};

template <typename T, typename MemberPtr>
constexpr auto field(const char* name, MemberPtr pointer) {
  return FieldDescriptor<T, MemberPtr>{name, pointer};
}

// Overload for deduced class type from member pointer.
template <typename C, typename M>
constexpr auto field(const char* name, M C::*pointer) {
  return FieldDescriptor<C, M C::*>{name, pointer};
}

// meta<T> — specialize for each struct to list its fields.
// Default: empty (no fields).
template <typename T>
struct meta {
  static constexpr auto fields = std::make_tuple();
};

// ── Binary encode/decode helpers ──────────────────────────────────────────

namespace detail {

inline void write_bytes(std::vector<uint8_t>& buffer,
                        const void* data,
                        size_t num_bytes) {
  const auto* ptr = static_cast<const uint8_t*>(data);
  buffer.insert(buffer.end(), ptr, ptr + num_bytes);
}

inline size_t read_bytes(const uint8_t* buffer,
                         size_t offset,
                         void* output,
                         size_t num_bytes) {
  std::memcpy(output, buffer + offset, num_bytes);
  return offset + num_bytes;
}

// ── Encode primitives ────────────────────────────────────────────────────

inline void encode_field(std::vector<uint8_t>& buffer, uint8_t value) {
  buffer.push_back(value);
}

inline void encode_field(std::vector<uint8_t>& buffer, bool value) {
  buffer.push_back(value ? 1 : 0);
}

inline void encode_field(std::vector<uint8_t>& buffer, int16_t value) {
  write_bytes(buffer, &value, sizeof(value));
}

inline void encode_field(std::vector<uint8_t>& buffer, uint16_t value) {
  write_bytes(buffer, &value, sizeof(value));
}

inline void encode_field(std::vector<uint8_t>& buffer, uint32_t value) {
  write_bytes(buffer, &value, sizeof(value));
}

inline void encode_field(std::vector<uint8_t>& buffer, uint64_t value) {
  write_bytes(buffer, &value, sizeof(value));
}

inline void encode_field(std::vector<uint8_t>& buffer,
                         const std::string& string_val) {
  auto length = static_cast<uint32_t>(string_val.size());
  write_bytes(buffer, &length, sizeof(length));
  write_bytes(buffer, string_val.data(), string_val.size());
}

inline void encode_field(std::vector<uint8_t>& buffer,
                         const std::vector<std::string>& vector_in) {
  auto count = static_cast<uint32_t>(vector_in.size());
  write_bytes(buffer, &count, sizeof(count));
  for (const auto& string_val : vector_in) {
    encode_field(buffer, string_val);
  }
}

inline void encode_field(std::vector<uint8_t>& buffer,
                         const std::vector<uint8_t>& vector_in) {
  auto count = static_cast<uint32_t>(vector_in.size());
  write_bytes(buffer, &count, sizeof(count));
  write_bytes(buffer, vector_in.data(), vector_in.size());
}

// Forward declaration for struct encoding (used by vector<T> below).
template <typename T>
void encode_struct(std::vector<uint8_t>& buffer, const T& object);

// Encode a vector of structs that have glz::meta<T> specializations.
template <typename T>
  requires(std::tuple_size_v<decltype(meta<T>::fields)> > 0)
inline void encode_field(std::vector<uint8_t>& buffer,
                         const std::vector<T>& vector_in) {
  auto count = static_cast<uint32_t>(vector_in.size());
  write_bytes(buffer, &count, sizeof(count));
  for (const auto& item : vector_in) {
    encode_struct(buffer, item);
  }
}

// Encode a map<string, vector<uint8_t>>.
inline void encode_field(
    std::vector<uint8_t>& buffer,
    const std::map<std::string, std::vector<uint8_t>>& mapping) {
  auto count = static_cast<uint32_t>(mapping.size());
  write_bytes(buffer, &count, sizeof(count));
  for (const auto& [key, value] : mapping) {
    encode_field(buffer, key);
    encode_field(buffer, value);
  }
}

// ── Decode primitives ────────────────────────────────────────────────────

inline size_t decode_field(const uint8_t* buffer,
                           size_t offset,
                           uint8_t& value) {
  value = buffer[offset];
  return offset + 1;
}

inline size_t decode_field(const uint8_t* buffer, size_t offset, bool& value) {
  value = buffer[offset] != 0;
  return offset + 1;
}

inline size_t decode_field(const uint8_t* buffer,
                           size_t offset,
                           int16_t& value) {
  return read_bytes(buffer, offset, &value, sizeof(value));
}

inline size_t decode_field(const uint8_t* buffer,
                           size_t offset,
                           uint16_t& value) {
  return read_bytes(buffer, offset, &value, sizeof(value));
}

inline size_t decode_field(const uint8_t* buffer,
                           size_t offset,
                           uint32_t& value) {
  return read_bytes(buffer, offset, &value, sizeof(value));
}

inline size_t decode_field(const uint8_t* buffer,
                           size_t offset,
                           uint64_t& value) {
  return read_bytes(buffer, offset, &value, sizeof(value));
}

inline size_t decode_field(const uint8_t* buffer,
                           size_t offset,
                           std::string& string_val) {
  uint32_t length{};
  offset = read_bytes(buffer, offset, &length, sizeof(length));
  string_val.assign(reinterpret_cast<const char*>(buffer + offset), length);
  return offset + length;
}

inline size_t decode_field(const uint8_t* buffer,
                           size_t offset,
                           std::vector<std::string>& vector_in) {
  uint32_t count{};
  offset = read_bytes(buffer, offset, &count, sizeof(count));
  vector_in.resize(count);
  for (uint32_t index = 0; index < count; ++index) {
    offset = decode_field(buffer, offset, vector_in[index]);
  }
  return offset;
}

inline size_t decode_field(const uint8_t* buffer,
                           size_t offset,
                           std::vector<uint8_t>& vector_in) {
  uint32_t count{};
  offset = read_bytes(buffer, offset, &count, sizeof(count));
  vector_in.assign(buffer + offset, buffer + offset + count);
  return offset + count;
}

// Forward declaration for struct decoding (used by vector<T> below).
template <typename T>
size_t decode_struct(const uint8_t* buffer, size_t offset, T& object);

// Decode a vector of structs that have glz::meta<T> specializations.
template <typename T>
inline size_t decode_field(const uint8_t* buffer,
                           size_t offset,
                           std::vector<T>& vector_in) {
  uint32_t count{};
  offset = read_bytes(buffer, offset, &count, sizeof(count));
  vector_in.resize(count);
  for (uint32_t index = 0; index < count; ++index) {
    offset = decode_struct(buffer, offset, vector_in[index]);
  }
  return offset;
}

// Decode a map<string, vector<uint8_t>>.
inline size_t decode_field(
    const uint8_t* buffer,
    size_t offset,
    std::map<std::string, std::vector<uint8_t>>& mapping) {
  uint32_t count{};
  offset = read_bytes(buffer, offset, &count, sizeof(count));
  mapping.clear();
  for (uint32_t index = 0; index < count; ++index) {
    std::string key;
    offset = decode_field(buffer, offset, key);
    std::vector<uint8_t> value_vec;
    offset = decode_field(buffer, offset, value_vec);
    mapping[std::move(key)] = std::move(value_vec);
  }
  return offset;
}

// ── Struct encode/decode via meta<T>::fields ─────────────────────────────

template <typename T, typename Tuple, std::size_t... I>
void encode_impl(std::vector<uint8_t>& buffer,
                 const T& object,
                 const Tuple& fields,
                 std::index_sequence<I...> /*unused*/) {
  (encode_field(buffer, object.*(std::get<I>(fields).pointer)), ...);
}

template <typename T, typename Tuple, std::size_t... I>
size_t decode_impl(const uint8_t* buffer,
                   size_t offset,
                   T& object,
                   const Tuple& fields,
                   std::index_sequence<I...> /*unused*/) {
  ((offset =
        decode_field(buffer, offset, object.*(std::get<I>(fields).pointer))),
   ...);
  return offset;
}

template <typename T>
inline void encode_struct(std::vector<uint8_t>& buffer, const T& object) {
  constexpr auto fields = meta<T>::fields;
  constexpr auto num_fields = std::tuple_size_v<decltype(fields)>;
  encode_impl(buffer, object, fields, std::make_index_sequence<num_fields>{});
}

template <typename T>
inline size_t decode_struct(const uint8_t* buffer, size_t offset, T& object) {
  constexpr auto fields = meta<T>::fields;
  constexpr auto num_fields = std::tuple_size_v<decltype(fields)>;
  return decode_impl(buffer, offset, object, fields,
                     std::make_index_sequence<num_fields>{});
}

}  // namespace detail

// Encode a struct to a byte buffer using its meta<T>::fields.
template <typename T>
inline std::vector<uint8_t> encode(const T& object) {
  std::vector<uint8_t> buffer;
  detail::encode_struct(buffer, object);
  return buffer;
}

// Decode a struct from a byte buffer using its meta<T>::fields.
// Returns the offset past the consumed bytes.
template <typename T>
inline size_t decode(const uint8_t* buffer, size_t offset, T& object) {
  return detail::decode_struct(buffer, offset, object);
}

}  // namespace glz