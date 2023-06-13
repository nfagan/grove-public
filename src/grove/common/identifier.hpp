#pragma once

#define GROVE_INTEGER_IDENTIFIER_EQUALITY(class_name, id_member)              \
  friend inline bool operator==(const class_name& a, const class_name& b) {   \
    return a.id_member == b.id_member;                                        \
  }                                                                           \
  friend inline bool operator!=(const class_name& a, const class_name& b) {   \
    return a.id_member != b.id_member;                                        \
  }

#define GROVE_INTEGER_IDENTIFIER_INEQUALITIES(class_name, id_member)          \
  friend inline bool operator<(const class_name& a, const class_name& b) {    \
    return a.id_member < b.id_member;                                         \
  }                                                                           \
  friend inline bool operator<=(const class_name& a, const class_name& b) {   \
    return a.id_member <= b.id_member;                                        \
  }                                                                           \
  friend inline bool operator>(const class_name& a, const class_name& b) {    \
    return a.id_member > b.id_member;                                         \
  }                                                                           \
  friend inline bool operator>=(const class_name& a, const class_name& b) {   \
    return a.id_member >= b.id_member;                                        \
  }

#define GROVE_INTEGER_IDENTIFIER_STD_HASH(hash_class_name, identifier_class_name, id_member) \
  struct hash_class_name {                                                                   \
    std::size_t operator()(const identifier_class_name& self) const noexcept {               \
      return std::hash<decltype(self.id_member)>{}(self.id_member);                          \
    }                                                                                        \
  };

#define GROVE_INTEGER_IDENTIFIER_IS_VALID(id_member) \
  bool is_valid() const {                            \
    return (id_member) != 0;                         \
  }