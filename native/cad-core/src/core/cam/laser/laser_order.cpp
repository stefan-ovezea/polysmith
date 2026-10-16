#include "core/cam/laser/laser_order.h"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <vector>

namespace polysmith::core::laser {

namespace {

using cam2d::XY;
using cam2d::xy_length;

// ── Refinement tuning ────────────────────────────────────────────────
// A contour can carry hundreds of sampled vertices and a layer thousands of
// contours, so scoring every vertex of every candidate is quadratic in both.
// The entry set is capped to evenly spaced samples: the emitter decides the
// real pierce point, so an approximate entry distance is the right granularity
// for ORDERING decisions.
constexpr size_t kMaxEntryCandidates = 8;
// Relocation sweeps.  Each sweep is O(n · window) exact local evaluations.
constexpr int kRefinementSweeps = 4;
// Above this many loops the relocation window is limited, so a pathological
// layer cannot turn generation quadratic.  2k-contour layers (a real copper
// layer) get the full window.
constexpr size_t kFullWindowLimit = 6000;
constexpr size_t kLimitedWindow = 64;
constexpr double kImprovementEpsilonMm = 1e-6;

// Distance from `point` to the closest vertex of the loop's sampled
// contour — the cheapest stand-in for the entry jump (the actual
// pierce point is chosen per loop by the emitter).
double closest_vertex_distance(const PlannedLoop& loop, const XY& point) {
  double best = std::numeric_limits<double>::max();
  for (const auto& sample : loop.samples) {
    best = std::min(best, xy_length(sample.x - point.x, sample.y - point.y));
  }
  return best;
}

// The loop's last sampled point — where the walk ends (lead-out aside),
// i.e. where the NEXT rapid starts from.
XY loop_end_point(const PlannedLoop& loop) {
  if (loop.samples.empty()) {
    return loop.centroid;
  }
  return loop.samples.back();
}

/// What the refinement pass needs per loop, computed once: where the walk ends
/// and a capped set of candidate entry points.
struct LoopAnchor {
  XY end;
  std::vector<XY> entries;
};

LoopAnchor make_anchor(const PlannedLoop& loop) {
  LoopAnchor anchor;
  anchor.end = loop_end_point(loop);
  if (loop.samples.empty()) {
    anchor.entries.push_back(loop.centroid);
    return anchor;
  }
  const size_t stride =
      std::max<size_t>(1, loop.samples.size() / kMaxEntryCandidates);
  for (size_t i = 0; i < loop.samples.size(); i += stride) {
    anchor.entries.push_back(loop.samples[i]);
    if (anchor.entries.size() >= kMaxEntryCandidates) {
      break;
    }
  }
  if (anchor.entries.empty()) {
    anchor.entries.push_back(loop.samples.front());
  }
  return anchor;
}

double entry_distance(const LoopAnchor& anchor, const XY& from) {
  double best = std::numeric_limits<double>::max();
  for (const XY& entry : anchor.entries) {
    best = std::min(best, xy_length(entry.x - from.x, entry.y - from.y));
  }
  return best;
}

}  // namespace

void order_laser_loops(std::vector<PlannedLoop>& loops,
                       const std::string& cut_order) {
  if (loops.empty()) {
    return;
  }

  if (cut_order == "by_area") {    // Legacy: regions sorted by their OUTER (shallowest) loop area;
    // within a group holes before their outer, ties by area.
    std::unordered_map<size_t, int> groupMinDepth;
    for (const auto& loop : loops) {
      auto it = groupMinDepth.find(loop.group);
      if (it == groupMinDepth.end() || loop.depth < it->second) {
        groupMinDepth[loop.group] = loop.depth;
      }
    }
    std::unordered_map<size_t, double> groupArea;
    for (const auto& loop : loops) {
      if (loop.depth == groupMinDepth[loop.group]) {
        groupArea[loop.group] = loop.area;
      }
    }
    std::stable_sort(loops.begin(), loops.end(), [&](const auto& a,
                                                     const auto& b) {
      if (groupArea[a.group] != groupArea[b.group]) {
        return groupArea[a.group] < groupArea[b.group];
      }
      if (a.is_hole != b.is_hole) {
        return a.is_hole;  // holes first
      }
      return a.area < b.area;
    });
    return;
  }

  // Depth-disciplined order: deepest loops first, so every hole (and
  // every enclosed part) is released before its container.  Within a
  // depth bucket:
  //   inner_first      — prefer a loop whose parent was JUST cut
  //                      (adjacency), then the smallest area;
  //   nearest_neighbor — greedy nearest-entry-point travel, then a
  //                      relocation refinement (below).
  int maxDepth = 0;
  for (const auto& loop : loops) {
    maxDepth = std::max(maxDepth, loop.depth);
  }
  std::vector<std::vector<size_t>> buckets(maxDepth + 1);
  for (size_t i = 0; i < loops.size(); ++i) {
    buckets[loops[i].depth].push_back(i);
  }

  std::vector<size_t> order;
  order.reserve(loops.size());
  XY current{0.0, 0.0};  // travel starts at the machine origin
  // Contiguous [begin, end) span of each depth bucket in `order`: the region
  // the refinement pass may permute.
  std::vector<std::pair<size_t, size_t>> bucket_spans;
  for (int depth = maxDepth; depth >= 0; --depth) {
    const size_t span_begin = order.size();
    std::vector<size_t> remaining = buckets[depth];
    while (!remaining.empty()) {
      size_t best = 0;
      if (cut_order == "nearest_neighbor") {
        double bestDist = std::numeric_limits<double>::max();
        for (size_t k = 0; k < remaining.size(); ++k) {
          const double d = closest_vertex_distance(loops[remaining[k]],
                                                   current);
          if (d < bestDist) {
            bestDist = d;
            best = k;
          }
        }
      } else {
        // inner_first: group/parent adjacency with the last cut loop,
        // then smallest area.
        best = 0;
        const size_t last = order.empty() ? kNoParent : order.back();
        const auto adjacent = [&](const PlannedLoop& candidate) {
          if (last == kNoParent) {
            return false;
          }
          const auto& lastLoop = loops[last];
          // Same region group as the last cut, the last cut's
          // enclosing loop, or a sibling of the last cut.
          return candidate.group == lastLoop.group ||
                 candidate.parent == last ||
                 candidate.parent == lastLoop.parent;
        };
        for (size_t k = 1; k < remaining.size(); ++k) {
          const auto& candidate = loops[remaining[k]];
          const auto& chosen = loops[remaining[best]];
          const bool candidateAdjacent = adjacent(candidate);
          const bool chosenAdjacent = adjacent(chosen);
          if (candidateAdjacent != chosenAdjacent) {
            if (candidateAdjacent) {
              best = k;
            }
            continue;
          }
          if (candidate.area < chosen.area) {
            best = k;
          }
        }
      }
      order.push_back(remaining[best]);
      current = loop_end_point(loops[remaining[best]]);
      remaining.erase(remaining.begin() + best);
    }
    bucket_spans.emplace_back(span_begin, order.size());
  }

  // ── Relocation refinement (nearest_neighbor only) ────────────────
  // Greedy nearest-neighbour is typically 20–40 % above a refined tour, and on
  // a PCB layer — thousands of independent contours, all in one depth bucket —
  // that travel is the dominant cost of the job.
  //
  // A loop may move ANYWHERE inside its own depth bucket: an enclosing loop is
  // always exactly one bucket deeper and therefore already cut, so no
  // relocation can invert the hole-before-container rule.  That is what lets
  // this be a plain local search rather than a constrained one.
  //
  // inner_first is deliberately NOT refined: its order encodes parent
  // adjacency, not travel minimisation.
  if (cut_order == "nearest_neighbor" && order.size() > 2) {
    std::vector<LoopAnchor> anchors;
    anchors.reserve(loops.size());
    for (const auto& loop : loops) {
      anchors.push_back(make_anchor(loop));
    }
    const XY origin{0.0, 0.0};
    const size_t count = order.size();
    // Which bucket a position belongs to, so a move never crosses one.
    std::vector<size_t> bucket_of(count, 0);
    for (size_t span = 0; span < bucket_spans.size(); ++span) {
      for (size_t position = bucket_spans[span].first;
           position < bucket_spans[span].second; ++position) {
        bucket_of[position] = span;
      }
    }
    const size_t window =
        count <= kFullWindowLimit ? count : kLimitedWindow;

    // Where the head stands before cutting position `position`: the previous
    // loop's end, or the machine origin for the very first loop.
    const auto previous_end = [&](size_t position) {
      return position == 0 ? origin : anchors[order[position - 1]].end;
    };
    // Rapid travel of arriving at `target_position` from `from`.
    const auto arrival = [&](const XY& from, size_t target_position) {
      return entry_distance(anchors[order[target_position]], from);
    };

    for (int sweep = 0; sweep < kRefinementSweeps; ++sweep) {
      bool improved = false;
      for (size_t i = 0; i < count; ++i) {
        const auto move_improves = [&](size_t j) {
          // Relocating `i` to sit immediately after `j`; j == i or j == i-1
          // would be a no-op (or overlap the very edges being re-scored).
          if (j == i || j + 1 == i || bucket_of[j] != bucket_of[i]) {
            return false;
          }
          const bool has_next_of_i = i + 1 < count;
          const bool has_next_of_j = j + 1 < count;

          // Removal: (prev(i) → i) + (i → i+1) become (prev(i) → i+1).
          double removal = -arrival(previous_end(i), i);
          if (has_next_of_i) {
            removal -= arrival(anchors[order[i]].end, i + 1);
            removal += arrival(previous_end(i), i + 1);
          }
          // Insertion: the edge (j → next(j)) becomes (j → i) + (i → next(j)).
          // `next(j)` is unchanged by removing a loop at i, because
          // j + 1 != i is guaranteed above.
          double insertion = 0.0;
          if (has_next_of_j) {
            insertion -= arrival(anchors[order[j]].end, j + 1);
          }
          insertion += arrival(anchors[order[j]].end, i);
          if (has_next_of_j) {
            insertion += arrival(anchors[order[i]].end, j + 1);
          }

          if (removal + insertion >= -kImprovementEpsilonMm) {
            return false;
          }
          // Apply.  After erasing `i`, the element originally at j sits at
          // j-1 when i < j, so "immediately after j" is index j; otherwise j
          // is unaffected and the slot after it is j+1.
          const size_t value = order[i];
          if (i < j) {
            order.erase(order.begin() + static_cast<long>(i));
            order.insert(order.begin() + static_cast<long>(j), value);
          } else {
            order.erase(order.begin() + static_cast<long>(i));
            order.insert(order.begin() + static_cast<long>(j) + 1, value);
          }
          return true;
        };

        const size_t last = std::min(count - 1, i + window);
        for (size_t j = i > window ? i - window : 0; j <= last; ++j) {
          if (move_improves(j)) {
            improved = true;
            break;
          }
        }
      }
      if (!improved) {
        break;
      }
    }
  }

  std::vector<PlannedLoop> reordered;
  reordered.reserve(loops.size());
  for (const size_t index : order) {
    reordered.push_back(std::move(loops[index]));
  }
  loops = std::move(reordered);
}

}  // namespace polysmith::core::laser
