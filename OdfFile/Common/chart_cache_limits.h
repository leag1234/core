// Chart caches are optional derived data. Refuse oversized expansions before allocation.
#pragma once
#include <cstddef>
#include <limits>
namespace cpdoccore { namespace chart_cache {
constexpr std::size_t max_points = 1000000;
// One chart shares this allowance across series, labels, categories and domains.
class budget
{
    std::size_t used_ = 0;
public:
    bool consume(std::size_t count)
    {
        if (count > max_points - used_) return false;
        used_ += count;
        return true;
    }
};
inline bool bounded_range(std::size_t c1, std::size_t r1, std::size_t c2,
                          std::size_t r2, std::size_t& count)
{
    if (c2 < c1 || r2 < r1 || c2 == (std::numeric_limits<std::size_t>::max)() ||
        r2 == (std::numeric_limits<std::size_t>::max)() ||
        c2 - c1 >= max_points || r2 - r1 >= max_points)
        return false;
    const std::size_t columns = c2 - c1 + 1, rows = r2 - r1 + 1;
    if (columns > max_points / rows) return false;
    count = columns * rows;
    return true;
}
} }
