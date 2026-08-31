#include "core/cam/laser/laser_order.h"

#include <algorithm>
#include <limits>
#include <unordered_map>

namespace polysmith::core::laser {

namespace {

using cam2d::XY;
using cam2d::xy_length;

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
  //   nearest_neighbor — greedy nearest-entry-point travel.
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
  for (int depth = maxDepth; depth >= 0; --depth) {
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
  }

  std::vector<PlannedLoop> reordered;
  reordered.reserve(loops.size());
  for (const size_t index : order) {
    reordered.push_back(std::move(loops[index]));
  }
  loops = std::move(reordered);
}

}  // namespace polysmith::core::laser
