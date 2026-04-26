# Testing Cellulon

This is a design doc for the project's testing strategy. It commits to a specific approach (in-house, exception-recovery-based, no third-party deps) and lays out the pieces. Sibling docs: `on_aggressive_asserts.md`, `on_error_adts_v2.md`.

## Constraints we're designing under

- **No third-party test libraries.** ~99% no, ~1% emergency-only. Catch2, doctest, GoogleTest, Boost.Test — all out.
- **Cross-platform clang.** Linux (primary) + MSYS2/MinGW (Windows). No `#ifdef _WIN32` for the core mechanism.
- **Aggressive-assert philosophy.** `CL_ASSERT` aborts in production (`on_aggressive_asserts.md`). Tests need to exercise paths that fire `CL_ASSERT` *without* killing the test runner.
- **Visual / experimental simulation.** Most simulation behavior isn't reliably testable. Cell rules, mating, mutation, fitness curves all change weekly. Don't try to test them.
- **Hand-rolled fundamental types** (`Option`, `Result`, future `Grid`/`GameRNG`/etc.) need real, comprehensive tests. They have stable, contractually correct behavior.

The goal is "8/10 tests passed" output for the things that are testable, with the lowest sensible amount of mechanism.

## Why exception-based and not longjmp / subprocess / configurable handler

This was the question. The short version:

- **longjmp** is C-flavored, skips destructors, has subtle UB rules with non-trivial stack frames, and is what GoogleTest uses *only* because Google bans exceptions in production. We don't share that constraint.
- **Subprocess-per-test** is rock-solid but ~50 lines of `posix_spawn` / `CreateProcess` platform abstraction per platform, plus 10–100x slower than in-process. Right tool for true crash-isolation; wrong tool for "an assertion fired in code under test". We'll keep it as an optional tier 6 if a real need shows up.
- **Runtime configurable assertion handler** (function pointer / thread-local) works but bolts on more state than necessary and creates a "did I forget to install the handler before this test" failure mode.
- **Compile-time exception swap** is what every modern C++ test framework actually does. Test target compiles with `CELLULON_TESTING` defined; `assertion_failed` throws. Production target undefined; `assertion_failed` aborts as today. The test runner catches at the per-test boundary. ~30 lines of mechanism. Portable. No platform `#ifdef`. Production binary identical to today.

We pick the last one. Production has zero exception-based control flow. Tests use exceptions only as the recovery mechanism at the test boundary, which is the one place exceptions are unambiguously good.

## The four-tier strategy

These compose. We don't pick one.

### Tier 1 — `static_assert` / compile-time tests

Most of the `Option`/`Result` API is `constexpr`-eligible. Exercise it at compile time.

```cpp
namespace cellulon::compile_tests {

constexpr bool t_some_unwrap() {
    Option<i32> x = Some(42);
    return x.is_some() && x.unwrap() == 42;
}
static_assert(t_some_unwrap());

constexpr bool t_map_chain() {
    return Some(3).map([](i32 v){ return v * 2; }).unwrap() == 6;
}
static_assert(t_map_chain());

}
```

Properties:

- Failure happens at compile time, points at the exact line.
- Every `xmake build` re-runs the suite — there is no "I forgot to run tests" failure mode.
- Forces our types to actually be `constexpr` on happy paths, which is good hygiene.
- Lives in dedicated headers/files (`src/test/static_test_option.hpp`?) included into the test target only, OR scattered in headers right next to what they verify.

Limit: cannot exercise paths that hit `assertion_failed` (it's not `constexpr`). Use tier 2 for those.

Convention: `static_assert([]{ ... }())` blocks are fine but slow to read. Named lambda-returning-bool helpers + a `static_assert(t_name())` line is the preferred shape.

### Tier 2 — In-house runtime framework

This is where the bulk of testing happens. Single test binary, exception-based assertion recovery, simple registry, ~100 lines total.

#### The split assertion handler

`utils.hpp` currently has `assertion_failed` calling `std::abort()`. We split it:

```cpp
// utils.hpp (sketch — modifies the existing assertion_failed)
namespace cellulon {

#ifdef CELLULON_TESTING

struct AssertionFailure {
    const char*          expression;
    std::string          message;        // owning, may come from std::format
    std::source_location where;
};

[[noreturn]] inline void assertion_failed(
    const char* expression,
    const char* message,
    std::source_location loc = std::source_location::current())
{
    throw AssertionFailure{expression, std::string{message}, loc};
}

#else

[[noreturn]] inline void assertion_failed(
    const char* expression,
    const char* message,
    std::source_location loc = std::source_location::current())
{
    std::print("\n[ASSERTION FAILED] WHERE? => {}:{} in function {}:\n",
               loc.file_name(), loc.line(), loc.function_name());
    std::print("    WHAT? => `{}`;\n", expression);
    std::print("    WHY? => `{}`.\n\n", message);
    std::abort();
}

#endif

}
```

Notes:

- `[[noreturn]]` is fine on a throwing function. The attribute means "doesn't return to the caller normally". Throwing satisfies that — control transfers via the exception machinery, not via a `return`.
- The production behavior is byte-for-byte identical to the existing implementation. Same prints, same `abort`. Adding `CELLULON_TESTING` doesn't change anything for the main binary.
- `AssertionFailure` is the *only* exception type the project defines, and it lives behind `CELLULON_TESTING`. Production code can't even name it.

#### Test registration

Static registration via Meyers-singleton registry. Avoids the static-init-order fiasco: the registry is constructed on first access, regardless of which TU registers first.

```cpp
// src/test/test.hpp
#pragma once
#include <vector>
#include <string_view>
#include <source_location>
#include "../utils.hpp"

namespace cellulon::testing {

struct TestCase {
    std::string_view name;
    void (*fn)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

struct Registrar {
    Registrar(std::string_view name, void (*fn)()) {
        registry().push_back({name, fn});
    }
};

void record_failure(std::source_location loc, std::string msg);

} // namespace cellulon::testing

#define CL_DETAIL_CONCAT_INNER(a, b) a##b
#define CL_DETAIL_CONCAT(a, b)       CL_DETAIL_CONCAT_INNER(a, b)
#define CL_DETAIL_TESTFN(line)       CL_DETAIL_CONCAT(_cl_test_fn_, line)
#define CL_DETAIL_TESTREG(line)      CL_DETAIL_CONCAT(_cl_test_reg_, line)

#define CL_TEST(name)                                                         \
    static void CL_DETAIL_TESTFN(__LINE__)();                                 \
    static ::cellulon::testing::Registrar CL_DETAIL_TESTREG(__LINE__){        \
        #name, &CL_DETAIL_TESTFN(__LINE__)};                                  \
    static void CL_DETAIL_TESTFN(__LINE__)()
```

Usage:

```cpp
// src/test/test_option.cpp
#include "../option.hpp"
#include "test.hpp"

CL_TEST(option_some_unwrap) {
    Option<i32> x = Some(42);
    CL_EXPECT(x.is_some(), "Some(42) should be is_some");
    CL_EXPECT_EQ(x.unwrap(), 42);
}

CL_TEST(option_unwrap_none_panics) {
    Option<i32> x = None;
    CL_EXPECT_PANIC(x.unwrap());
}
```

Two `CL_TEST(...)` blocks per file. No fixtures. No setup/teardown. If a test needs a registry/grid/whatever, it constructs them itself — the per-test cost in a headless suite is microseconds, and shared fixtures are a maintenance headache.

#### `CL_EXPECT*` family

The expect macros record failures *without aborting*, so a single test can collect multiple. (Unlike `CL_ASSERT`, which is for production code and stops dead.)

```cpp
#define CL_EXPECT(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) [[unlikely]]                                             \
            ::cellulon::testing::record_failure(                              \
                std::source_location::current(),                              \
                std::format("CL_EXPECT({}) failed: {}", #cond, msg));         \
    } while (false)

#define CL_EXPECT_EQ(a, b)                                                    \
    do {                                                                      \
        auto&& _a = (a);                                                      \
        auto&& _b = (b);                                                      \
        if (!(_a == _b)) [[unlikely]]                                         \
            ::cellulon::testing::record_failure(                              \
                std::source_location::current(),                              \
                std::format("CL_EXPECT_EQ({} == {}) failed: "                 \
                            "lhs = {}, rhs = {}", #a, #b, _a, _b));           \
    } while (false)

#define CL_EXPECT_NE(a, b) /* symmetric */

#define CL_EXPECT_PANIC(expr)                                                 \
    do {                                                                      \
        bool _panicked = false;                                               \
        try { (void)(expr); }                                                 \
        catch (::cellulon::AssertionFailure const&) { _panicked = true; }     \
        if (!_panicked) [[unlikely]]                                          \
            ::cellulon::testing::record_failure(                              \
                std::source_location::current(),                              \
                "expected " #expr " to trigger CL_ASSERT, didn't");           \
    } while (false)
```

`CL_EXPECT_EQ` requires both sides to be `std::formattable` — fine for arithmetic, strings, and anything we provide a `std::formatter` for. For types without `std::format` support, fall back to `CL_EXPECT(a == b, "...")` and write the message yourself.

`CL_EXPECT_PANIC` is the payoff of the exception design — testing "this should fire `CL_ASSERT`" is now a five-line macro, no longjmp, no subprocess, no platform code.

#### The runner

```cpp
// src/test/main.cpp
#include "test.hpp"
#include <print>
#include <string>
#include <vector>

namespace {
    thread_local std::vector<std::string> g_failures;
}

namespace cellulon::testing {
    void record_failure(std::source_location loc, std::string msg) {
        g_failures.push_back(std::format(
            "    {}:{}: {}", loc.file_name(), loc.line(), msg));
    }
}

int main(int argc, char** argv) {
    using namespace cellulon;

    int passed = 0;
    int failed = 0;

    for (auto const& tc : testing::registry()) {
        g_failures.clear();

        try {
            tc.fn();
        } catch (AssertionFailure const& af) {
            g_failures.push_back(std::format(
                "    {}:{}: CL_ASSERT(`{}`) fired during test: {}",
                af.where.file_name(), af.where.line(),
                af.expression, af.message));
        } catch (std::exception const& e) {
            g_failures.push_back(std::format(
                "    uncaught std::exception: {}", e.what()));
        } catch (...) {
            g_failures.push_back("    uncaught unknown exception");
        }

        if (g_failures.empty()) {
            std::println("[PASS] {}", tc.name);
            ++passed;
        } else {
            std::println("[FAIL] {}", tc.name);
            for (auto const& f : g_failures) std::println("{}", f);
            ++failed;
        }
    }

    std::println("\nSummary: {} passed, {} failed ({} total)",
                 passed, failed, passed + failed);
    return failed == 0 ? 0 : 1;
}
```

That's the entire framework. ~100 lines including the assertion handler split.

#### Filtering, ordering, repeats

Defer until annoying:

- **Filter**: a `--filter=substring` arg that skips tests whose name doesn't contain the substring. Trivial, ~5 lines, add when the suite gets big enough that running everything is annoying.
- **Order**: registration order = run order. No randomization, no shuffling. Determinism is the point.
- **Repeat**: `--repeat=N` for stress-testing flaky tests. Add only when we have a flaky test, which means we already have a problem.

#### What this approach can't catch

Honest list:

- A genuine segfault inside code under test kills the runner. Same as Catch2. Tier 5 (signal/SEH net) is the partial mitigation.
- An `std::abort()` from raylib / EnTT / libc internals bypasses the throw. Same fix as above.
- Stack overflow. Same.
- Anything that calls `std::terminate()` directly.

For all of these the real fix is a process boundary (tier 6, deferred).

### Tier 3 — Simulation smoke / soak

For the simulation itself, "headless run for N frames, no asserts fire, invariants hold":

```cpp
// src/test/smoke_sim.cpp
CL_TEST(sim_smoke_1000_frames) {
    entt::registry reg;
    sim::create_initial_population(reg, /*seed=*/42);
    for (i32 i = 0; i < 1000; ++i) sim::step(reg);
    CL_EXPECT(reg.view<sim::Cell>().size() > 0, "all cells died");
    // optional: invariant checks on positions, energy, ranges
}
```

This is the bridge between unit tests and integration tests. It catches:

- Update-order issues, accumulator drift, NaN propagation.
- Off-by-ones in grid wrap.
- Component-lifetime bugs that unit tests on individual systems miss.
- Determinism regressions: same seed should produce the same trajectory.

Cost: a few seconds per smoke test. Fast enough to run on every build.

Implication for production code: `sim::step()` must be callable without `InitWindow()` having run. raylib's headless mode (or just not calling any rendering function) makes this straightforward, but the design has to keep simulation logic separate from rendering. Worth pinning as an architectural rule now while the sim is still a stub.

### Tier 4 — Sanitizers

`xmake.lua` already has commented-out ASan/UBSan flags. Re-enable them *for the test target only*:

```lua
target("cellulon-tests")
    -- ... (see XMake section below)
    if is_mode("debug") then
        add_cxxflags("-fno-omit-frame-pointer", "-fsanitize=address,undefined")
        add_ldflags("-fsanitize=address,undefined", {force = true})
    end
```

Sanitizers under tests catch use-after-free and UB inside `Option`/`Result` storage *before* it shows up as weird simulation behavior. Highest single-flag bug-catching value in the entire stack.

LeakSanitizer is part of ASan; `lsan.supp` already exists at the project root. Tests should run clean.

### Tier 5 — Signal / SEH safety net (optional, deferred)

When a real segfault fires, it'd be nice to know which test was running. ~30 lines:

```cpp
namespace { thread_local std::string_view current_test_name; }

// On POSIX, install handlers for SIGSEGV/SIGFPE/SIGABRT that print
// current_test_name and re-raise. On Windows, SetUnhandledExceptionFilter
// or equivalent.
```

The runner sets `current_test_name = tc.name` before each `tc.fn()`. Doesn't recover — the runner still dies — but you get useful output.

Skip until a real segfault wastes your time.

### Tier 6 — Subprocess isolation (optional, deferred)

For tests that need to genuinely crash (e.g. testing the production-mode `assertion_failed` calls `abort` and not `throw`, or testing UB-on-purpose paths under sanitizer):

- POSIX: `posix_spawn` + `waitpid`.
- Windows: `CreateProcess` + `GetExitCodeProcess`.
- ~50 lines of platform abstraction.

We re-exec ourselves with `--run-only=name` to run a single test in a child process; the parent observes exit code.

Tier 1 + tier 2 covers >99% of what we want. Don't build tier 6 until a real callsite demands it.

## XMake integration

Two targets. Production unchanged, test target added.

```lua
-- xmake.lua additions

target("cellulon-tests")
    set_kind("binary")
    -- All production sources except main.cpp (test target has its own main).
    add_files("src/*.cpp|main.cpp", "src/test/*.cpp")
    add_packages("raylib", "entt")
    add_defines("CELLULON_TESTING")
    -- clang has exceptions on by default; this is documentation. If we
    -- ever globally pass -fno-exceptions to the production target, the
    -- test target stays correct.
    add_cxxflags("-fexceptions", {tools = {"clang"}})

    -- Same libc++ wiring as the main target.
    if is_plat("linux", "macosx", "mingw") then
        add_cxxflags("-stdlib=libc++", {tools = {"clang"}})
        add_ldflags("-stdlib=libc++", {tools = {"clang"}, force = true})
        if is_plat("linux") then
            add_syslinks("c++abi")
        end
    end

    -- Sanitizers in debug mode (test target only).
    if is_mode("debug") then
        add_cxxflags("-fno-omit-frame-pointer",
                     "-fsanitize=address,undefined")
        add_ldflags("-fsanitize=address,undefined", {force = true})
    end
```

Workflow:

```sh
xmake build cellulon-tests        # build tests
xmake run cellulon-tests          # run them
xmake run cellulon-tests --filter=option   # later, when filter is added
```

Production target (`target("cellulon")`) is unchanged. `CELLULON_TESTING` is undefined in the production binary; `assertion_failed` aborts as today; no exceptions are ever thrown.

## File layout

```
src/
  utils.hpp           # CL_ASSERT, CL_DBG_ASSERT, assertion_failed (split by #ifdef)
  option.hpp          # Option<T> (when implemented)
  result.hpp          # Result<T,E>, Error (when implemented)
  ...
  test/
    test.hpp          # CL_TEST, CL_EXPECT*, CL_EXPECT_PANIC, registry, Registrar
    main.cpp          # the runner
    test_option.hpp   # static_assert tier 1 tests for Option (optional split)
    test_option.cpp   # CL_TEST tier 2 tests for Option
    test_result.cpp
    test_error.cpp
    test_grid.cpp
    test_rng.cpp
    smoke_sim.cpp     # tier 3
```

Convention: one test file per module, named `test_<module>.cpp`. Smoke/soak tests in `smoke_*.cpp`. Static-assert-only files end in `_static.hpp` if we want them isolated; otherwise inline alongside the runtime tests.

## Conventions

- **Test names** are `snake_case`, descriptive, prefixed by the module: `option_some_unwrap`, `result_err_propagates_through_and_then`, `grid_in_bounds_corners`. Long names are fine; they're the failure message.
- **One behavior per `CL_TEST`.** If a test name needs an "and" or a comma, split it.
- **No fixtures.** Each test constructs what it needs. Test bodies are short enough that the duplication doesn't matter.
- **Determinism is non-negotiable.** Tests that touch `GameRNG` use a fixed seed (`GameRNG(42)`). Tests must not call `GetTime()` or anything wall-clock-dependent. Tests must not require an `InitWindow()`.
- **Test files include relative paths** (`#include "../option.hpp"`) since they live in `src/test/`. We don't add `src/` to the include path for the test target — the relative paths make it obvious where the dependency points.

## What we deliberately don't test

Pin this list, because future-you will be tempted:

- Cell behavior, mating rules, mutation outcomes, fitness curves, energy decay rates. These are tunable; they change weekly. Unit tests would calcify a number that's supposed to be a knob.
- Visual output: colors, line thickness, layout, FPS counters.
- Anything where "correct" means "looks plausible in motion".
- Allocation counts, exact cycle counts, cache behavior. We're not optimizing yet, and these tests are flaky by nature.

For the simulation's behavioral tuning the right tool is `xmake run cellulon` and watching it. Tests that try to assert "after 1000 frames the population is between X and Y" will break every time you tweak a constant.

What we *do* test on the simulation side, via tier 3 smoke runs:

- It doesn't crash.
- It doesn't fire `CL_ASSERT`.
- It produces *some* surviving population (not zero, not infinity).
- Same seed produces same trajectory (determinism).

Anything stronger than "not catastrophic" belongs in a notebook or a debug overlay, not the test suite.

## Implementation order (when we eventually build this)

1. Split `assertion_failed` in `utils.hpp` with `#ifdef CELLULON_TESTING`. Add the `AssertionFailure` struct.
2. Create `src/test/test.hpp` with the registry, `Registrar`, `CL_TEST`, `CL_EXPECT*`, `CL_EXPECT_PANIC` macros.
3. Create `src/test/main.cpp` with the runner.
4. Add the `cellulon-tests` target to `xmake.lua`.
5. Write a smoke `CL_TEST(framework_self_check)` in `src/test/test_meta.cpp` that exercises `CL_EXPECT_EQ`, `CL_EXPECT_PANIC`, and a deliberate failure to verify the FAIL path.
6. Once `Option`/`Result` are implemented, populate `test_option.cpp` and `test_result.cpp`.
7. Add tier 4 sanitizer flags to the test target.
8. Defer tiers 5 and 6 indefinitely.

Steps 1–5 are the minimum viable framework. The rest is content.

## Open questions / future work

- **Pretty-printing for `CL_EXPECT_EQ` of user types.** Currently requires `std::formatter<T>`. We could provide a fallback that prints `???` for unformattable types instead of a compile error, but the compile error is arguably the right behavior — it forces us to implement `std::formatter` for types we want to debug, which we want anyway.
- **A `CL_EXPECT_THROWS_TYPE(expr, T)` for testing specific exception types.** Not needed yet because we don't throw outside `AssertionFailure`. Add if we ever introduce more exception types in test mode.
- **CI integration.** When this gets a CI, run `xmake build && xmake run cellulon-tests` on every push. Sanitizers on. No third-party CI needed; a single GitHub Actions workflow with one shell step is sufficient.
- **A `CL_EXPECT_OK(result_expr)` / `CL_EXPECT_ERR(result_expr)` shorthand** specifically for `Result`-returning expressions. Trivial to add once `Result` exists.
- **Headless raylib for tier 3.** If any smoke test needs raylib initialized, `SetConfigFlags(FLAG_WINDOW_HIDDEN)` before `InitWindow` works on most platforms. Verify when we hit it.

## What success looks like

`xmake run cellulon-tests` prints:

```
[PASS] option_some_unwrap
[PASS] option_none_default
[FAIL] option_unwrap_none_panics
    src/test/test_option.cpp:18: expected x.unwrap() to trigger CL_ASSERT, didn't
[PASS] result_ok_propagates
...

Summary: 27 passed, 1 failed (28 total)
```

Production binary is identical to today (no exceptions, no test code, no overhead). One test breaking doesn't hide the others. `CL_EXPECT_PANIC` works without subprocess machinery. The mechanism is small enough to fit in one's head.

If any of those stop being true, the design failed and we revisit.
