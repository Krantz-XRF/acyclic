#include <filesystem>
#include <iostream>
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
#include <spdlog/spdlog.h>

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
llvm::cl::OptionCategory category{"acyclic"};

inline std::string className(clang::QualType t) {
  auto record = t.getTypePtr()->getAs<clang::RecordType>();
  return fmt::format(
    fmt::fg(fmt::terminal_color::green), "{}",
    record ? record->getDecl()->getName().str() : t.getAsString());
}

inline std::string varName(const clang::NamedDecl& d) {
  return fmt::format(fmt::fg(fmt::terminal_color::cyan), "{}", d.getName());
}

inline std::string funcName(const clang::FunctionDecl& d) {
  return fmt::format(fmt::fg(fmt::terminal_color::bright_cyan), "{}",
                     d.getName());
}

struct SourceLoc {
  std::filesystem::path file;
  size_t line;
  size_t column;

  static SourceLoc from(const clang::SourceLocation& loc,
                        const clang::SourceManager& sm) {
    auto file = std::filesystem::path{sm.getFilename(loc).str()};
    return {std::filesystem::relative(file), sm.getSpellingLineNumber(loc),
            sm.getSpellingColumnNumber(loc)};
  }
};
}  // namespace

template <>
struct fmt::formatter<SourceLoc> : formatter<std::string> {
  template <typename FormatContext>
  auto format(const SourceLoc& s, FormatContext& ctx) {
    auto r = fmt::format("{}:{}:{}", s.file.string(), s.line, s.column);
    return formatter<std::string>::format(r, ctx);
  }
};

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

    fmt::print("{}: function {} referenced {} as {}::{}.\n",
               SourceLoc::from(function->getLocation(), *sm),
               funcName(*function), className(memberType),
               className(parentType), varName(*member));
  }
};
}  // namespace ast

int main(int argc, const char* argv[]) {
  tool::CommonOptionsParser options{argc, argv, category};
  tool::ClangTool tool{options.getCompilations(), options.getSourcePathList()};

  ast::MemberAccessHandler handler;
  ast::MatchFinder finder;
  finder.addMatcher(ast::memberAccess, &handler);

  return tool.run(tool::newFrontendActionFactory(&finder).get());
}
