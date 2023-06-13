#pragma once

#include <cstdint>
#include <functional>

namespace grove::io {

struct RegisteredString {
  struct Hash {
    inline std::size_t operator()(const RegisteredString& ident) const noexcept {
      return std::hash<uint64_t>{}(ident.id);
    }
  };

  friend inline bool operator<(const RegisteredString& a, const RegisteredString& b) {
    return a.id < b.id;
  }
  friend inline bool operator<=(const RegisteredString& a, const RegisteredString& b) {
    return a.id <= b.id;
  }
  friend inline bool operator>(const RegisteredString& a, const RegisteredString& b) {
    return a.id > b.id;
  }
  friend inline bool operator>=(const RegisteredString& a, const RegisteredString& b) {
    return a.id >= b.id;
  }
  friend inline bool operator==(const RegisteredString& a, const RegisteredString& b) {
    return a.id == b.id;
  }
  friend inline bool operator!=(const RegisteredString& a, const RegisteredString& b) {
    return a.id != b.id;
  }

  uint64_t id{};
};

struct ReferenceIdentifier {
  struct Hash {
    inline std::size_t operator()(const ReferenceIdentifier& ident) const noexcept {
      return std::hash<uint64_t>{}(ident.id);
    }
  };

  friend inline bool operator<(const ReferenceIdentifier& a, const ReferenceIdentifier& b) {
    return a.id < b.id;
  }
  friend inline bool operator<=(const ReferenceIdentifier& a, const ReferenceIdentifier& b) {
    return a.id <= b.id;
  }
  friend inline bool operator>(const ReferenceIdentifier& a, const ReferenceIdentifier& b) {
    return a.id > b.id;
  }
  friend inline bool operator>=(const ReferenceIdentifier& a, const ReferenceIdentifier& b) {
    return a.id >= b.id;
  }
  friend inline bool operator==(const ReferenceIdentifier& a, const ReferenceIdentifier& b) {
    return a.id == b.id;
  }
  friend inline bool operator!=(const ReferenceIdentifier& a, const ReferenceIdentifier& b) {
    return a.id != b.id;
  }

  uint64_t id{};
};

}