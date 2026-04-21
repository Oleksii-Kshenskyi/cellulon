A minimal replacement that actually serves you:

```cpp
// assert.hpp
#pragma once
#include <cstdlib>
#include <print>
#include <source_location>

namespace cellulon {

// Always-on assert - survives release builds
// Use for invariants where failure means the simulation result is garbage
[[noreturn]] inline void assertion_failed(
    const char* expression,
    const char* message,
    std::source_location loc = std::source_location::current())
{
    std::println(stderr, "[ASSERT FAILED] {}:{} in {}",
                 loc.file_name(), loc.line(), loc.function_name());
    std::println(stderr, "  Expression: {}", expression);
    std::println(stderr, "  Message:    {}", message);
    std::abort();
}

} // namespace cellulon

// Two tiers:

// CELLULON_ASSERT - always on, for invariants that matter in release
#define CELLULON_ASSERT(expr, msg) \
    do { if (!(expr)) [[unlikely]] \
        ::cellulon::assertion_failed(#expr, msg); \
    } while (false)

// CELLULON_DEBUG_ASSERT - compiled out in release, for hot-path checks
#ifdef NDEBUG
    #define CELLULON_DEBUG_ASSERT(expr, msg) do {} while (false)
#else
    #define CELLULON_DEBUG_ASSERT(expr, msg) CELLULON_ASSERT(expr, msg)
#endif
```
Now you can do:
```cpp
CELLULON_ASSERT(cell_size > 0, "cell_size must be positive before grid init");
CELLULON_ASSERT(genome.genes.size() == Genome::kLength, "genome corrupted");
CELLULON_DEBUG_ASSERT(grid.in_bounds(pos.i, pos.j), "entity escaped grid bounds");
```