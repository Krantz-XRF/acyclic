#include "ref_graph.h"

#include <spdlog/spdlog.h>

void cycle_detector::detect() {
  for (const auto& e : graph) {
    const auto& x = e.first;
    if (!is_visited(x)) visit(x);
  }
}

void cycle_detector::visit(const std::string& x) {
  spdlog::trace("{}visiting {} ...", std::string(indent * 2, ' '), x);
  ++indent;
  if (is_visited(x)) {
    spdlog::warn("circular reference detected for {}.", x);
    auto p = std::find(refs.cbegin(), refs.cend(), x);
    auto last = p++;
    for (; p != refs.cend(); ++p, ++last) {
      fmt::memory_buffer buf;
      fmt::format_to(buf, "along the ref chain {} -> {}:\n", *last, *p);
      std::set<std::string> funcs, fields;
      std::set<SourceLoc> locs;
      for (const auto& [f, ctx] : graph[*last][*p]) {
        funcs.insert(ctx.function);
        fields.insert(f);
        locs.insert(ctx.location);
        fmt::format_to(buf, "  - {}::{} in ({}:{})\n", *last, f, ctx.location,
                       ctx.function);
      }
      fmt::format_to(buf,
                     "  total: {} field(s), {} function(s), {} location(s).",
                     fields.size(), funcs.size(), locs.size());
      spdlog::info(buf.data());
    }
  } else {
    refs.push_back(x);
    visited.insert(x);
    for (const auto& [t, info] : graph[x]) visit(t);
    refs.pop_back();
  }
  --indent;
}

bool cycle_detector::is_visited(const std::string& x) {
  return visited.find(x) != visited.cend();
}
