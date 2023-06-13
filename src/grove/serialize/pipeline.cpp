#include "pipeline.hpp"
#include "scan.hpp"
#include "parse.hpp"
#include "declare.hpp"
#include "resolve.hpp"
#include "StringRegistry.hpp"
#include "grove/common/common.hpp"

GROVE_NAMESPACE_BEGIN

namespace {

using namespace io;

auto err_ast(std::vector<io::ParseError>&& errs) {
  return either::make_right<MaybeAst>(std::move(errs));
}

auto ok_ast(io::ast::Ast&& ast) {
  return either::make_left<MaybeAst>(std::move(ast));
}

} //  anon

io::MaybeAst io::make_ast(const std::string& source, io::StringRegistry& registry) {
  auto scan_res = io::scan(source.c_str(), source.size());
  if (!scan_res.success) {
    return err_ast(std::move(scan_res.errors));
  }

  io::ParseInfo info{registry};
  auto parse_res = parse(scan_res.tokens, info);
  if (!parse_res.success) {
    return err_ast(std::move(parse_res.errors));
  }

  auto decl_res = io::declare_aggregates(parse_res.ast);
  if (!decl_res.success) {
    return err_ast(std::move(decl_res.errors));
  }

  auto resolve_res = io::resolve_references(parse_res.ast, decl_res.declarations);
  if (!resolve_res.success) {
    return err_ast(std::move(resolve_res.errors));
  }

  return ok_ast(std::move(parse_res.ast));
}

GROVE_NAMESPACE_END
