#pragma once

#include "ast.hpp"
#include "grove/common/Optional.hpp"
#include "grove/math/vector.hpp"
#include "grove/math/matrix.hpp"
#include <array>

namespace grove::io {

class StringRegistry;

namespace impl {

template <typename T>
struct WhichNumberType {};

template <>
struct WhichNumberType<double> {
  static constexpr ast::NumberNode::Type type = ast::NumberNode::Type::Double;
  static inline double extract(const io::ast::NumberNode& node) {
    return node.double_value;
  }
  static inline std::unique_ptr<ast::NumberNode> wrap(double v, Token tok) {
    return std::make_unique<ast::NumberNode>(tok, v);
  }
};

template <>
struct WhichNumberType<float> {
  static constexpr ast::NumberNode::Type type = ast::NumberNode::Type::Double;
  static inline float extract(const io::ast::NumberNode& node) {
    return float(node.double_value);
  }
  static inline std::unique_ptr<ast::NumberNode> wrap(float v, Token tok) {
    return std::make_unique<ast::NumberNode>(tok, double(v));
  }
};

template <>
struct WhichNumberType<int64_t> {
  static constexpr ast::NumberNode::Type type = ast::NumberNode::Type::Int64;
  static inline int64_t extract(const io::ast::NumberNode& node) {
    return node.int_value;
  }
  static inline std::unique_ptr<ast::NumberNode> wrap(int64_t v, Token tok) {
    return std::make_unique<ast::NumberNode>(tok, v);
  }
};

template <typename T>
std::vector<ast::BoxedNode> to_array_elements(const T* data, std::size_t size) {
  std::vector<ast::BoxedNode> elements(size);
  for (size_t i = 0; i < size; i++) {
    elements[i] = impl::WhichNumberType<T>::wrap(data[i], Token::null());
  }
  return elements;
}

template <typename T, int N>
Optional<std::array<T, N>> parse_array(const ast::Node& node) {
  if (auto array = node.as<const ast::ArrayNode>()) {
    if (array->elements.size() != N) {
      return NullOpt{};
    }

    std::array<T, N> res;

    for (int i = 0; i < N; i++) {
      if (auto num = array->elements[i]->as<const ast::NumberNode>()) {
        if (num->type == impl::WhichNumberType<T>::type) {
          res[i] = impl::WhichNumberType<T>::extract(*num);
        } else {
          return NullOpt{};
        }
      } else {
        return NullOpt{};
      }
    }

    return Optional<std::array<T, N>>(std::move(res));

  } else {
    return NullOpt{};
  }
}

template <typename T>
Optional<T> parse_number(const ast::Node& node) {
  if (auto num = node.as<const ast::NumberNode>()) {
    if (num->type == impl::WhichNumberType<T>::type) {
      return Optional<T>(impl::WhichNumberType<T>::extract(*num));
    }
  }

  return NullOpt{};
}

} //  impl

std::unique_ptr<ast::ArrayNode> to_array(const Mat4f& mat);
std::unique_ptr<ast::ArrayNode> to_array(const Vec3f& v3);
std::unique_ptr<ast::NumberNode> to_number(float v);
std::unique_ptr<ast::NumberNode> to_number(int64_t v);

Optional<Vec3f> parse_vec3(const ast::Node& node);
Optional<Mat4f> parse_mat4(const ast::Node& node);
Optional<float> parse_float(const ast::Node& node);
Optional<int64_t> parse_int64(const ast::Node& node);
const std::string* parse_string_ptr(const ast::Node& node, const StringRegistry& registry);

}