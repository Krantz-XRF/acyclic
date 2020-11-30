#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>

#include <fmt/color.h>
#include <fmt/compile.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <gsl/gsl_assert>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "ref_graph.h"

namespace tool = clang::tooling;

template <>
struct fmt::formatter<llvm::StringRef> : formatter<std::string_view> {
  template <typename FormatContext>
  auto format(llvm::StringRef s, FormatContext& ctx) {
    using parent = formatter<std::string_view>;
    return parent::format(std::string_view{s.data(), s.size()}, ctx);
  }
};

namespace {
namespace cl = llvm::cl;

cl::OptionCategory category{"acyclic"};

cl::opt<std::string> verbose{
  "verbose", cl::cat{category}, cl::init("info"), cl::value_desc{"level"},
  cl::desc{"Verbosity: [trace, debug, info, warning, error, critical, off]"}};

std::optional<spdlog::level::level_enum> parse_verbosity(std::string_view s) {
  static constexpr std::array levels = SPDLOG_LEVEL_NAMES;
  const auto p = std::find(begin(levels), end(levels), s);
  if (p == end(levels)) return std::nullopt;
  return static_cast<spdlog::level::level_enum>(p - begin(levels));
}

inline std::string className(clang::QualType t) {
  auto record = t.getTypePtr()->getAs<clang::RecordType>();
  return record ? record->getDecl()->getName().str() : t.getAsString();
}

inline std::string varName(const clang::NamedDecl& d) {
  return d.getName().str();
}

inline std::string funcName(const clang::FunctionDecl& d) {
  return d.getName().str();
}

inline std::string classNameC(clang::QualType t) {
  return fmt::format(fmt::fg(fmt::terminal_color::green), "{}", className(t));
}

inline std::string varNameC(const clang::NamedDecl& d) {
  return fmt::format(fmt::fg(fmt::terminal_color::cyan), "{}", varName(d));
}

inline std::string funcNameC(const clang::FunctionDecl& d) {
  return fmt::format(fmt::fg(fmt::terminal_color::bright_cyan), "{}",
                     funcName(d));
}
}  // namespace

namespace ast {
using namespace clang::ast_matchers;

auto memberAccess =
  functionDecl(
    unless(isExpansionInSystemHeader()),
    hasBody(forEachDescendant(
      memberExpr(
        hasObjectExpression(hasType(qualType().bind("parentType"))),
        hasDeclaration(
          fieldDecl(
            hasType(
              qualType(optionally(hasDescendant(templateSpecializationType(
                         hasDeclaration(namedDecl(hasName("shared_ptr"))),
                         hasTemplateArgument(
                           0, refersToType(qualType().bind("sharedType")))))))
                .bind("memberType")))
            .bind("memberDecl")))
        .bind("access"))))
    .bind("function");

class MemberAccessHandler : public MatchFinder::MatchCallback {
 public:
  void run(const MatchFinder::MatchResult& result) override {
    using namespace clang;
    const auto sm = result.SourceManager;
    const auto function = result.Nodes.getNodeAs<FunctionDecl>("function");
    const auto access = result.Nodes.getNodeAs<MemberExpr>("access");
    const auto member = result.Nodes.getNodeAs<NamedDecl>("memberDecl");
    auto parentType = *result.Nodes.getNodeAs<QualType>("parentType");
    auto memberType = [&result] {
      if (auto sharedType = result.Nodes.getNodeAs<QualType>("sharedType"))
        return *sharedType;
      return *result.Nodes.getNodeAs<QualType>("memberType");
    }();
    Expects(access && member);

    if (access->isArrow())
      if (const auto pointer = parentType.getTypePtr()->getAs<PointerType>())
        parentType = pointer->getPointeeType();

    spdlog::debug("{}: function {} referenced {} as {}::{}.",
                  SourceLoc::from(function->getLocation(), *sm),
                  funcNameC(*function), classNameC(memberType),
                  classNameC(parentType), varNameC(*member));

    add_to(graph, classNameC(parentType), classNameC(memberType),
           varNameC(*member), funcNameC(*function),
           SourceLoc::from(function->getLocation(), *sm));
  }

  ref_graph graph;
};
}  // namespace ast

int main(int argc, const char* argv[]) {
  tool::CommonOptionsParser options{argc, argv, category};
  tool::ClangTool tool{options.getCompilations(), options.getSourcePathList()};

  set_default_logger(spdlog::stderr_color_st("acyclic"));
  spdlog::set_pattern("%n: %^%l:%$ %v");
  if (const auto v = parse_verbosity(verbose)) spdlog::set_level(*v);

  ast::MemberAccessHandler handler;
  ast::MatchFinder finder;
  finder.addMatcher(ast::memberAccess, &handler);

  auto err = tool.run(tool::newFrontendActionFactory(&finder).get());
  if (err) return err;

  detect_cyclic_ref(handler.graph);
}
