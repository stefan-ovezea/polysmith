#pragma once

// Binary payload encoding for the viewport mesh arrays (phase E4).  JSON number
// arrays cost ~20 bytes per float; encoding a float32/uint32 blob as base64 is
// 3-5x smaller and decodes straight into a JS Float32Array/Uint32Array on the
// UI side, so the parse-and-widen pass disappears too.
//
// Endianness: little-endian, which is what every shipped target uses AND what
// the JS typed arrays the UI builds assume.  A big-endian build would break the
// round-trip, so guard it.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace polysmith::protocol {

inline std::string base64_encode(const std::uint8_t* data, std::size_t length) {
  static const char* table =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((length + 2) / 3) * 4);
  for (std::size_t i = 0; i < length; i += 3) {
    const std::uint32_t a = data[i];
    const std::uint32_t b = i + 1 < length ? data[i + 1] : 0;
    const std::uint32_t c = i + 2 < length ? data[i + 2] : 0;
    const std::uint32_t n = (a << 16) | (b << 8) | c;
    out.push_back(table[(n >> 18) & 0x3f]);
    out.push_back(table[(n >> 12) & 0x3f]);
    out.push_back(i + 1 < length ? table[(n >> 6) & 0x3f] : '=');
    out.push_back(i + 2 < length ? table[n & 0x3f] : '=');
  }
  return out;
}

// Encode a list of doubles as a base64 float32 blob.  Doubles are narrowed to
// float (the UI only ever builds Float32Array, so the extra precision was
// never consumed).  Returns "" for an empty list.
inline std::string encode_f32(const std::vector<double>& values) {
  if (values.empty()) {
    return "";
  }
  std::vector<std::uint8_t> bytes;
  bytes.reserve(values.size() * 4);
  for (double value : values) {
    const float f = static_cast<float>(value);
    std::uint32_t bits = 0;
    std::memcpy(&bits, &f, 4);
    bytes.push_back(static_cast<std::uint8_t>(bits & 0xff));
    bytes.push_back(static_cast<std::uint8_t>((bits >> 8) & 0xff));
    bytes.push_back(static_cast<std::uint8_t>((bits >> 16) & 0xff));
    bytes.push_back(static_cast<std::uint8_t>((bits >> 24) & 0xff));
  }
  return base64_encode(bytes.data(), bytes.size());
}

// Encode a list of ints as a base64 uint32 blob.  Returns "" for an empty list.
inline std::string encode_u32(const std::vector<int>& values) {
  if (values.empty()) {
    return "";
  }
  std::vector<std::uint8_t> bytes;
  bytes.reserve(values.size() * 4);
  for (int value : values) {
    const std::uint32_t u = static_cast<std::uint32_t>(value);
    bytes.push_back(static_cast<std::uint8_t>(u & 0xff));
    bytes.push_back(static_cast<std::uint8_t>((u >> 8) & 0xff));
    bytes.push_back(static_cast<std::uint8_t>((u >> 16) & 0xff));
    bytes.push_back(static_cast<std::uint8_t>((u >> 24) & 0xff));
  }
  return base64_encode(bytes.data(), bytes.size());
}

}  // namespace polysmith::protocol
