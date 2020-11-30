#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "comparator.h"

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

inline Ord compare(const SourceLoc& l, const SourceLoc& r) {
  return comparator::begin()
    .next(l.file, r.file)
    .next(l.line, r.line)
    .next(l.column, r.column)
    .end();
}

inline bool operator<(const SourceLoc& l, const SourceLoc& r) {
  return compare(l, r) == Ord::LT;
}

template <>
struct fmt::formatter<SourceLoc> : formatter<std::string> {
  template <typename FormatContext>
  auto format(const SourceLoc& s, FormatContext& ctx) {
    auto r = fmt::format("{}:{}:{}", s.file.string(), s.line, s.column);
    return formatter<std::string>::format(r, ctx);
  }
};

struct ref_context {
  std::string function;
  SourceLoc location;
};

using c_type = std::string;
using c_field = std::string;
using ref_info = std::multimap<c_field, ref_context>;
using ref_graph = std::map<c_type, std::map<c_type, ref_info>>;

inline void add_to(ref_graph& graph, c_type from, c_type to,
                   std::string fieldName, std::string func,
                   SourceLoc location) {
  spdlog::trace("adding ref {} -> {} (via {}) in ({}:{})", from, to, fieldName,
                location, func);
  graph[from][to].emplace(fieldName, ref_context{func, location});
}

class cycle_detector {
 public:
  cycle_detector(ref_graph& graph) : graph{graph} {}
  void detect();

 private:
  void visit(const std::string& x);
  bool is_visited(const std::string& x);

  ref_graph& graph;
  std::vector<std::string> refs;
  std::set<std::string> visited;

  int indent{0};
};

inline void detect_cyclic_ref(ref_graph& graph) {
  cycle_detector{graph}.detect();
}
