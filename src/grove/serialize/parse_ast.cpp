#include "parse_ast.hpp"
#include "StringRegistry.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

Optional<Vec3f> io::parse_vec3(const ast::Node& node) {
  if (auto array_res = impl::parse_array<float, 3>(node)) {
    Vec3f result;
    for (int i = 0; i < 3; i++) {
      result[i] = array_res.value()[i];
    }
    return Optional<Vec3f>(result);
  } else {
    return NullOpt{};
  }
}

Optional<Mat4f> io::parse_mat4(const ast::Node& node) {
  if (auto array_res = impl::parse_array<float, 16>(node)) {
    Mat4f result;
    std::memcpy(result.elements, array_res.value().data(), 16 * sizeof(float));
    return Optional<Mat4f>(result);
  } else {
    return NullOpt{};
  }
}

Optional<float> io::parse_float(const ast::Node& node) {
  return impl::parse_number<float>(node);
}

Optional<int64_t> io::parse_int64(const ast::Node& node) {
  return impl::parse_number<int64_t>(node);
}

const std::string* io::parse_string_ptr(const ast::Node& node, const StringRegistry& registry) {
  if (auto str = node.as<const ast::StringNode>()) {
    return &registry.get(str->str);
  } else {
    return nullptr;
  }
}

std::unique_ptr<io::ast::NumberNode> io::to_number(float v) {
  return std::make_unique<ast::NumberNode>(Token::null(), double(v));
}

std::unique_ptr<io::ast::NumberNode> io::to_number(int64_t v) {
  return std::make_unique<ast::NumberNode>(Token::null(), v);
}

std::unique_ptr<io::ast::ArrayNode> io::to_array(const Vec3f& v3) {
  float v[3] = {v3.x, v3.y, v3.z};
  auto elements = impl::to_array_elements<float>(v, 3);
  return std::make_unique<ast::ArrayNode>(Token::null(), std::move(elements));
}

std::unique_ptr<io::ast::ArrayNode> io::to_array(const Mat4f& mat) {
  auto elements = impl::to_array_elements<float>(mat.elements, 16);
  return std::make_unique<ast::ArrayNode>(Token::null(), std::move(elements));
}

GROVE_NAMESPACE_END
