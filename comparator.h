#pragma once

#include <cstdint>

enum class Ord : int8_t { LT = -1, EQ, GT };

template <typename T, typename U>
Ord compare(const T& x, const U& y) {
  if (x < y) return Ord::LT;
  if (x == y) return Ord::EQ;
  return Ord::GT;
}

class comparator {
 public:
  static comparator begin() { return {}; }

  template <typename T, typename U>
  comparator& next(const T& x, const U& y) {
    if (result == Ord::EQ) result = compare(x, y);
    return *this;
  }
  bool is(Ord x) const { return x == result; }
  Ord end() const { return result; }

 private:
  Ord result;
};
