#pragma once
#include <cstddef>

namespace pad {
// A UDP datagram is complete on its own. Do not carry a truncated packet
// into a later datagram, even when both arrived in the same frame.
inline bool validInSimDatagram(const unsigned char *data, std::size_t size) {
  if (size < 4)
    return false;
  for (std::size_t offset = 0; offset < size;) {
    const auto bytes = data[offset] * 4u;
    if (bytes < 4 || bytes > size - offset)
      return false;
    offset += bytes;
  }
  return true;
}
} // namespace pad
