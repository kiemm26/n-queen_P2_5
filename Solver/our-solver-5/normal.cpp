#include <array>
#include <bitset>
#include <cstdint>

#include "nqueens.h"

namespace {
using Mask = std::uint32_t;
using Word = std::uint64_t;
using Results = std::vector<std::vector<int>>;
int columnIndex(Mask q) {
  return static_cast<int>(std::bitset<32>(q - 1).count());
}
Mask reverseMask(Mask m, int n) {
  m = ((m & 0x5555u) << 1) | ((m >> 1) & 0x5555u);
  m = ((m & 0x3333u) << 2) | ((m >> 2) & 0x3333u);
  m = ((m & 0x0f0fu) << 4) | ((m >> 4) & 0x0f0fu);
  return (((m & 255u) << 8) | (m >> 8)) >> (16 - n);
}

// A path stores one column per row in four-bit fields. The two diagonal
// directions occupy separate 32-bit lanes and both advance by a left shift.
struct Layout {
  Word diagonals;
  Mask path;
};
struct StripGroup {
  Layout items[6];
  unsigned size;
};
// At most 6! permutations can use a fixed set of six columns.
struct HalfLayouts {
  Mask diagonals[720];
  Mask paths[720];
  unsigned size;
};
template <int N> struct StripSearch {
  static constexpr Mask board = (1u << N) - 1;
  // Index references only populated groups, with at most C(N,2)+C(N,3) entries.
  std::array<unsigned short, 1u << N> index;
  std::array<StripGroup, 1 + N *(N - 1) / 2 + N *(N - 1) * (N - 2) / 6> groups;
  unsigned groupCount = 0;

  std::vector<Word> matchesFound;
  std::size_t total = 0;

  // Adjacent entries are a path and its row reversal. Thus i ^ 1 picks
  // the other orientation without searching the group.
  void generateShortStrips(int row, Mask used, Word diagonals, Mask path,
                           Word reversedDiagonals, Mask reversedPath) {
    if (row >= 2 && path < reversedPath) {
      auto &g = groups[index[used]];
      g.items[g.size++] = {reversedDiagonals, reversedPath};
      g.items[g.size++] = {diagonals, path};
    }
    if (row == 3)
      return;
    for (Mask free = board ^ used; free; free &= free - 1) {
      Mask bit = free & -free;
      int col = columnIndex(bit);
      Word d = ((Word(bit) << 32) | (1u << (N - 1 - col))) << row;
      if (!(d & diagonals))
        generateShortStrips(
            row + 1, used | bit, diagonals | d, path | (col << (4 * row)),
            (reversedDiagonals << 1) | (d >> row), (reversedPath << 4) | col);
    }
  }

  template <int Height>
  void buildHalf(Mask set, HalfLayouts &result, int shift) {
    result.size = 0;
    // For equal-sized strips keep one order of the column partition.
    // Reversing a joined layout below recovers the opposite order.
    Mask subset = set;
    if constexpr (Height % 2 == 0)
      subset ^= set & -set;
    for (Mask first = subset; first; first &= first - 1) {
      for (Mask second = first & (first - 1); second; second &= second - 1) {
        Mask a = (first & -first) | (second & -second);
        if constexpr (Height == 6)
          a ^= subset;
        const auto &upperGroup = groups[index[a]];
        const auto &lowerGroup = groups[index[set ^ a]];
        for (unsigned j = 0; j < lowerGroup.size; j += 2) {
          for (unsigned i = 0; i < upperGroup.size; i += 2) {
            auto combine = [&](unsigned upper, unsigned lower) {
              const auto u = upperGroup.items[upper],
                         l = lowerGroup.items[lower];
              Word d = l.diagonals << (Height / 2);
              // Compact candidates in place. The next write reuses the slot
              // of a conflicting pair. This keeps the inner loop branch-free.
              unsigned k = result.size;
              const bool valid = (d & u.diagonals) == 0;
              Word attacks = (d | u.diagonals) >> shift;
              result.diagonals[k] =
                  ((attacks >> 16) & 0x7fff0000u) | (attacks & 0x7fffu);
              result.paths[k] = u.path | (l.path << (4 * (Height / 2)));
              if constexpr (Height % 2 == 0) {
                const auto ru = upperGroup.items[upper ^ 1],
                           rl = lowerGroup.items[lower ^ 1];
                Word rev =
                    (rl.diagonals | (ru.diagonals << (Height / 2))) >> shift;
                result.diagonals[k + 1] =
                    ((rev >> 16) & 0x7fff0000u) | (rev & 0x7fffu);
                result.paths[k + 1] = rl.path | (ru.path << (4 * (Height / 2)));
                result.size += 2 * unsigned(valid);
              } else
                result.size += unsigned(valid);
            };
            combine(i, j);
            combine(i + 1, j);
            combine(i, j + 1);
            combine(i + 1, j + 1);
          }
        }
      }
    }
  }

  // Next mask in numeric order having the same number of set bits.
  static Mask nextSet(Mask set) {
    const Mask bit = set & -set;
    const Mask carry = set + bit;
    return carry | (((carry ^ set) >> 2) >> columnIndex(bit));
  }

  Results run() {
    for (int i = 0; i < N; ++i) {
      for (int j = i + 1; j < N; ++j) {
        Mask pair = (1u << i) | (1u << j);
        index[pair] = ++groupCount;
        groups[groupCount].size = 0;
        for (int k = j + 1; k < N; ++k) {
          index[pair | (1u << k)] = ++groupCount;
          groups[groupCount].size = 0;
        }
      }
    }
    generateShortStrips(0, 0, 0, 0, 0, 0);
    HalfLayouts top, bottom;
    for (Mask a = (1u << (N / 2)) - 1; a < board; a = nextSet(a)) {
      Mask b = board ^ a, m = reverseMask(a, N);
      if (m < a)
        continue;
      Mask images = a == m ? 1 : 3;
      if constexpr (N % 2 == 0) {
        if (b < a || (board ^ m) < a)
          continue;
        if (b != m)
          images |= (b == (board ^ m) ? 4 : 12);
      }
      // On odd boards the middle queen belongs to the lower strip. Include
      // its column in each key so reflected partitions remain distinct.
      std::array<Mask, N> centerImages{};
      if constexpr (N % 2) {
        bool any = false;
        for (Mask cells = b; cells; cells &= cells - 1) {
          Mask q = cells & -cells;
          Mask lower = b ^ q, revLower = reverseMask(lower, N);
          int c = columnIndex(q), rev = N - 1 - c;
          std::array<Mask, 4> keys{(a << 4) | Mask(c), (m << 4) | Mask(rev),
                                   (lower << 4) | Mask(c),
                                   (revLower << 4) | Mask(rev)};
          unsigned imageMask = 1;
          for (int i = 1; i < 4; ++i) {
            if (keys[i] < keys[0]) {
              imageMask = 0;
              break;
            }
            bool duplicate = false;
            for (int j = 0; j < i; ++j)
              duplicate |= keys[i] == keys[j];
            if (!duplicate)
              imageMask |= 1u << i;
          }
          centerImages[c] = imageMask;
          any |= imageMask != 0;
        }
        if (!any)
          continue;
      }
      buildHalf<N / 2>(a, top, N / 2);
      buildHalf<N - N / 2>(b, bottom, 0);
      // A lower layout always exposes at least one diagonal across the split.
      // All-ones padding therefore conflicts with every lower layout.
      unsigned padded = (top.size + 7u) & ~7u;
      for (unsigned k = top.size; k < padded; ++k)
        top.diagonals[k] = 0x7fffffffu;
      auto first = matchesFound.size();
      for (unsigned j = 0; j < bottom.size; ++j) {
        Mask imageMask = images;
        if constexpr (N % 2)
          imageMask = centerImages[bottom.paths[j] & 15];
        if (!imageMask)
          continue;
        const Mask attacks = bottom.diagonals[j];
        const Word repeated = attacks | (Word(attacks) << 32);
        const Word highBits = 0x8000000080000000ull;
        for (unsigned start = 0; start < top.size; start += 8) {
          // Each 32-bit lane has its high bit clear. Subtracting overlap
          // from that bit retains it exactly when the intersection is empty.
          Word matches = 0;
          for (unsigned pair = 0; pair < 4; ++pair) {
            unsigned k = start + 2 * pair;
            Word both = top.diagonals[k] | (Word(top.diagonals[k + 1]) << 32);
            Word overlap = both & repeated;
            matches |= ((highBits - overlap) & highBits) >> (31 - pair);
          }
          while (matches) {
            Word bit = matches & -matches;
            unsigned position = std::bitset<64>(bit - 1).count();
            unsigned lane = 2 * (position & 3) + (position >> 5);
            matches ^= bit;

            matchesFound.push_back(top.paths[start + lane] |
                                   (Word(bottom.paths[j]) << (4 * (N / 2))) |
                                   (Word(imageMask) << (4 * N)));
            if constexpr (N % 2)
              total += std::bitset<4>(imageMask).count();
          }
        }
      }
      if constexpr (N % 2 == 0)
        total += (matchesFound.size() - first) * std::bitset<4>(images).count();
    }
    // The exact count comes from this invocation. Reserve the output once.
    Results results;
    results.reserve(total);
    for (auto hit : matchesFound) {
      Word path = hit;
      auto images = hit >> (4 * N);
      std::array<int, N> rows;
      for (auto &c : rows) {
        c = path & 15;
        path >>= 4;
      }
      results.emplace_back(rows.begin(), rows.end());
      if (images & 4)
        results.emplace_back(rows.rbegin(), rows.rend());
      if (images & 2) {
        for (auto &c : rows)
          c = N - 1 - c;
        results.emplace_back(rows.begin(), rows.end());
        if (images & 8)
          results.emplace_back(rows.rbegin(), rows.rend());
      }
    }
    return results;
  }
};

// With two rows left, the last column is forced. Adjacent remaining
// columns would put those two queens on a common diagonal.
template <int N, int Row>
void smallRows(Mask free, Mask left, Mask right, std::array<int, N> &path,
               Results &results) {
  Mask choices = free & ~(left | right);
  if constexpr (Row == 0)
    choices &= (1u << ((N + 1) / 2)) - 1;
  if constexpr (Row == N - 2)
    if (free & (free >> 1))
      return;
  while (choices) {
    Mask q = choices & -choices;
    choices ^= q;
    path[Row] = columnIndex(q);
    if constexpr (Row == N - 2) {
      Mask last = free ^ q;
      if (last & ((left << 1) | (right >> 1)))
        continue;
      path[Row + 1] = columnIndex(last);
      results.emplace_back(path.begin(), path.end());
      if (path[0] < N / 2) {
        auto &reflected = results.emplace_back(N);
        for (int r = 0; r < N; ++r)
          reflected[r] = N - 1 - path[r];
      }
    } else
      smallRows<N, Row + 1>(free ^ q, (left | q) << 1, (right | q) >> 1, path,
                            results);
  }
}

template <int N> Results small() {
  std::array<int, N> path;
  Results results;
  smallRows<N, 0>((1u << N) - 1, 0, 0, path, results);
  return results;
}

template <int N> Results searchStrips() {
  StripSearch<N> search;
  return search.run();
}
} // namespace

std::vector<std::vector<int>> solveNQueens(int n) {
  switch (n) {
  case 4:
    return small<4>();
  case 5:
    return small<5>();
  case 6:
    return small<6>();
  case 7:
    return small<7>();
  case 8:
    return small<8>();
  case 9:
    return small<9>();
  case 10:
    return searchStrips<10>();
  case 11:
    return searchStrips<11>();
  case 12:
    return searchStrips<12>();
  default:
    return {};
  }
}
