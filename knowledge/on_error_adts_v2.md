# Error ADTs for Cellulon — v2

This is a design doc, not an implementation. It supersedes `on_error_adts.md` (which had real bugs and reinvented stdlib types badly) and reflects the actual constraints for this project:

- `Option<T>` and `Result<T, E>` will be implemented from scratch, not aliased onto `std::optional` / `std::expected`.
- Naming is Rust-flavored and non-negotiable. No `transform`, no `value_or`, no `nullopt`, no `error()`.
- The implementation should be small enough to fully understand and audit. No surprising stdlib quirks across libc++ / libstdc++ / MSVC.
- These are intended to be the *only* hand-rolled fundamental types in Cellulon. Every line gets paid for.

## Why we're rolling our own

The honest reasons, in order:

1. **`std::optional` is full of papercuts that vary by stdlib.** Construction from `T` vs from `optional<T>`, copy-vs-move ambiguity, the in-place vs assign distinction, the special case for `bool`, and the historical (pre-C++26) absence of reference support. Different vendors made different tradeoffs and surfaced different diagnostics. We don't want to relearn this on every machine.
2. **Naming.** `transform` and `value_or` and `nullopt_t` are wrong-vibes for this project.
3. **Pedagogical.** Implementing them ourselves means we know exactly what happens at every assignment, every move, every destructor. No "what does my libc++ do here" moments.
4. **`unwrap()` should call `CL_ASSERT`, not throw.** Already non-negotiable from `on_aggressive_asserts.md`. Stdlib `value()` throws.
5. **The implementation cost is bounded.** Roughly ~250 lines for both types plus tests. Once written, they don't change.

What we're explicitly *not* getting from this:

- We're not getting better codegen. Modern stdlib `optional`/`expected` are excellent. Our hand-rolled versions will at best match them.
- We're not getting more features. Stdlib has more methods, more conversions, more SFINAE-guarded specializations.
- We're not getting compile-time speed. We're paying ~200 extra lines of templates per TU that uses these.

That's all fine. Velocity, taste, and comprehension matter more here than micro-optimization.

## Naming reference (Rust → here)

Non-negotiable. The right column is what we ship. The left column is for context only.

### `Option<T>`

| Rust                       | Cellulon                  | Notes                                  |
| -------------------------- | ------------------------- | -------------------------------------- |
| `Some(x)`                  | `Some(x)`                 | Free function, returns proxy.          |
| `None`                     | `None`                    | Global tag value, converts to any `Option<T>`. |
| `is_some()`                | `is_some()`               |                                        |
| `is_none()`                | `is_none()`               |                                        |
| `unwrap()`                 | `unwrap()`                | Calls `CL_ASSERT` on `None`.           |
| `expect(msg)`              | `expect(msg)`             | Same, with caller-supplied message.    |
| `unwrap_or(d)`             | `unwrap_or(d)`            |                                        |
| `unwrap_or_else(f)`        | `unwrap_or_else(f)`       | Lazy fallback.                         |
| `unwrap_or_default()`      | `unwrap_or_default()`     | Requires `T` default-constructible.    |
| `map(f)`                   | `map(f)`                  | `Option<U>` from `Option<T>`.          |
| `and_then(f)`              | `and_then(f)`             | `f: T -> Option<U>`.                   |
| `or_else(f)`               | `or_else(f)`              | `f: () -> Option<T>`.                  |
| `filter(p)`                | `filter(p)`               |                                        |
| `take()`                   | `take()`                  | Replaces `*this` with `None`.          |
| `replace(x)`               | `replace(x)`              | Returns the old value as `Option<T>`.  |
| `get_or_insert(x)`         | `get_or_insert(x)`        | Returns `T&`.                          |
| `get_or_insert_with(f)`    | `get_or_insert_with(f)`   |                                        |
| `inspect(f)`               | `inspect(f)`              | `f: T const&  -> void`. Returns `*this`. |
| `ok_or(e)`                 | `ok_or(e)`                | `Option<T> -> Result<T, E>`.           |
| `ok_or_else(f)`            | `ok_or_else(f)`           |                                        |

Deliberately left out for v1: `as_ref`, `as_deref`, `flatten`, `zip`, `xor`, `iter`. Add when needed.

### `Result<T, E>`

| Rust                   | Cellulon              | Notes                                              |
| ---------------------- | --------------------- | -------------------------------------------------- |
| `Ok(x)`                | `Ok(x)`               | Free function, returns proxy.                      |
| `Err(e)`               | `Err(e)`              | Free function, returns proxy.                      |
| `is_ok()`              | `is_ok()`             |                                                    |
| `is_err()`             | `is_err()`            |                                                    |
| `unwrap()`             | `unwrap()`            | `CL_ASSERT(is_ok)`. Reports the error in the message. |
| `unwrap_err()`         | `unwrap_err()`        | `CL_ASSERT(is_err)`.                               |
| `expect(msg)`          | `expect(msg)`         |                                                    |
| `expect_err(msg)`      | `expect_err(msg)`     |                                                    |
| `unwrap_or(d)`         | `unwrap_or(d)`        |                                                    |
| `unwrap_or_else(f)`    | `unwrap_or_else(f)`   | `f: E -> T`.                                       |
| `unwrap_or_default()`  | `unwrap_or_default()` |                                                    |
| `map(f)`               | `map(f)`              | `Result<T,E> -> Result<U,E>`.                      |
| `map_err(f)`           | `map_err(f)`          | `Result<T,E> -> Result<T,F>`.                      |
| `and_then(f)`          | `and_then(f)`         |                                                    |
| `or_else(f)`           | `or_else(f)`          |                                                    |
| `inspect(f)`           | `inspect(f)`          |                                                    |
| `inspect_err(f)`       | `inspect_err(f)`      |                                                    |
| `ok()`                 | `ok()`                | `Result<T,E> -> Option<T>`.                        |
| `err()`                | `err()`               | `Result<T,E> -> Option<E>`.                        |

Deliberately left out for v1: `as_ref`, `as_mut`, `iter`, `transpose`. Add when needed.

## Storage strategy

A hand-rolled discriminated union with explicit lifetime, not `std::variant`, not `std::optional`.

Reasons:

- `std::variant` brings its own quirks (`valueless_by_exception`, weird visit syntax, double-storage discriminator in some impls) and we'd be wrapping it anyway. Skip the middleman.
- A direct union gives us exact control over move-from state semantics (we want "moved-from = `None`/`Err` left intact" — see below).
- It's strictly less code than wrapping a stdlib type.

The shape:

```cpp
namespace cellulon {

template<typename T>
class Option {
    union { T value_; char none_{}; };
    bool has_value_ = false;
    // ...
};

template<typename T, typename E>
class Result {
    union { T value_; E error_; };
    bool is_ok_;
    // ...
};

} // namespace cellulon
```

Special members are written explicitly. They follow these rules:

- **Default ctor** of `Option` constructs `None`. `Result` has no default ctor — you must explicitly construct it `Ok(x)` or `Err(e)`. (Different from stdlib `expected`, deliberate: a defaulted `Result` is meaningless.)
- **Destructor** runs `std::destroy_at(&active_member)` only when needed. Defaulted (and trivial) when both `T` and `E` are trivially destructible — use the C++20 conditional `= default` trick:

  ```cpp
  constexpr ~Option() requires std::is_trivially_destructible_v<T> = default;
  constexpr ~Option() { if (has_value_) std::destroy_at(&value_); }
  ```

- **Copy / move** are conditionally trivial (`= default` when `T`/`E` are trivially copyable/movable; otherwise hand-written). Conditionally deleted when `T`/`E` are not copyable/movable.
- **Assignment** is the dangerous one — destroy-old-then-construct-new is the rule, with strong exception guarantees only when the active member's move ctor is `noexcept`. We will simply require move ctors to be `noexcept` and assert on this with a `static_assert` at the top of the class, instead of writing the strong-guarantee dance. Cellulon's component types can satisfy this trivially.
- **Move-from semantics**: after `std::move(opt).unwrap()` or `opt.take()`, `opt` becomes `None`. After `std::move(res).unwrap()`, the rule we'll pick is "the `T` is moved out; `is_ok_` stays true; `value_` is in a moved-from state". This matches stdlib `expected` and is the conservative choice. (Rust's `Result` is destroyed-by-move at the language level — we can't quite replicate that.)

Reference type support:

- `Option<T&>` is supported via partial specialization that stores a `T*`. Mutating ops (`replace`, `take`) work. `unwrap()` returns `T&`. We do this on purpose because `find_entity_at(...)` and similar EnTT-flavored APIs benefit from it.
- `Result<T&, E>` and `Result<T, E&>` — not supported in v1. Add only if a real callsite demands it.

## The construction problem (`Some` / `None` / `Ok` / `Err`)

This was where v1 was actively broken. Rust's `Some(x)` / `Err(e)` work because the language's type inference fills in the other variant's type from context. C++ doesn't have that, so a free function `Err(e)` returning `Result<???, decltype(e)>` can't possibly know `T`.

Solution: factory functions return *proxy types*, and `Option`/`Result` have implicit constructors from those proxies.

```cpp
namespace cellulon {

// ----- None -----

struct NoneType { explicit constexpr NoneType() = default; };
inline constexpr NoneType None{};

// ----- Some -----

template<typename T>
struct SomeProxy {
    T value;
};

template<typename T>
constexpr SomeProxy<std::decay_t<T>> Some(T&& v) {
    return SomeProxy<std::decay_t<T>>{std::forward<T>(v)};
}

// ----- Ok -----

template<typename T>
struct OkProxy {
    T value;
};

template<typename T>
constexpr OkProxy<std::decay_t<T>> Ok(T&& v) {
    return OkProxy<std::decay_t<T>>{std::forward<T>(v)};
}

// Allow bare `Ok()` for `Result<Unit, E>`.
struct Unit {};
inline constexpr OkProxy<Unit> Ok() { return OkProxy<Unit>{Unit{}}; }

// ----- Err -----

template<typename E>
struct ErrProxy {
    E value;
};

template<typename E>
constexpr ErrProxy<std::decay_t<E>> Err(E&& e) {
    return ErrProxy<std::decay_t<E>>{std::forward<E>(e)};
}

} // namespace cellulon
```

Then `Option`/`Result` accept these as converting constructors:

```cpp
template<typename T>
class Option {
public:
    constexpr Option(NoneType) noexcept : has_value_(false) {}
    Option& operator=(NoneType) noexcept { reset_to_none(); return *this; }

    template<typename U>
        requires std::constructible_from<T, U&&>
    constexpr Option(SomeProxy<U> s)
        : has_value_(true)
    {
        std::construct_at(&value_, std::move(s.value));
    }

    // ... copy/move/assign/etc.
};

template<typename T, typename E>
class Result {
public:
    template<typename U>
        requires std::constructible_from<T, U&&>
    constexpr Result(OkProxy<U> ok) : is_ok_(true) {
        std::construct_at(&value_, std::move(ok.value));
    }

    template<typename F>
        requires std::constructible_from<E, F&&>
    constexpr Result(ErrProxy<F> err) : is_ok_(false) {
        std::construct_at(&error_, std::move(err.value));
    }

    // ... copy/move/assign/etc.
};
```

Now this works in any function:

```cpp
Result<Config, Error> load_config(std::string_view path) {
    if (!exists(path)) return Err(Error{"file not found"});
    auto cfg = parse(path);
    if (!cfg) return Err(Error{"parse failed"});
    return Ok(*cfg);
}

Option<entt::entity> find_at(Grid const& g, XY p) {
    if (!g.in_bounds(p)) return None;
    if (auto e = g.entity_at(p); e != entt::null) return Some(e);
    return None;
}
```

Notes on the proxy approach:

- The proxy types are private-feeling but live in `cellulon::` because `Some`/`Ok`/`Err` need to be discoverable. They're not meant to be named explicitly.
- The proxy holds the value by value (not by reference) so it's safe to return from `Some(...)`. RVO/copy-elision and the proxy's move ctor handle the rest.
- Cross-type assignment (`Option<long> y = Some(5);`) Just Works because of the templated converting constructor.
- For non-copyable / move-only types, `Some(std::move(x))` is the spelling. We never silently copy.

## `Error` type

Default error type. Owning, with optional cause chain.

```cpp
namespace cellulon {

struct Error {
    std::string message;
    std::source_location where;
    std::unique_ptr<Error> cause;  // optional, nullable, nullable, nullable.

    explicit Error(
        std::string msg,
        std::source_location loc = std::source_location::current()
    ) : message(std::move(msg)), where(loc), cause(nullptr) {}

    // Chain: returns a new Error whose `cause` is `*this` (moved).
    Error context(
        std::string ctx,
        std::source_location loc = std::source_location::current()
    ) && {
        Error wrapped(std::move(ctx), loc);
        wrapped.cause = std::make_unique<Error>(std::move(*this));
        return wrapped;
    }

    // Walks the cause chain, prints "msg\n  caused by: msg\n  caused by: ..."
    std::string format() const;
};

} // namespace cellulon
```

Decisions baked in:

- `message` owns its `std::string`. No `string_view` traps.
- `cause` is `unique_ptr<Error>` so cause chains compose without copying. Single owner. `format()` walks the chain.
- The chained `context()` method takes `&&` so it always consumes. If you accidentally call it on an lvalue, you get a compile error — which is the correct outcome, because a non-consuming version would silently drop the chain.
- `source_location` is captured at every Error construction (including each `.context()` call), so the cause chain has location info at every level. This is a meaningful upgrade over Rust's `anyhow::Error` and is essentially free.

For typed errors (e.g. `enum class ParseErr { ... }`), `Result<T, ParseErr>` works fine — the default `E = Error` is just a default. We never assume `E == Error` in the implementation.

## Propagation: `CL_TRY` and `CL_TRY_OK`

We can't have `?`. The macro replacement, with `__COUNTER__` for hygiene:

```cpp
// Internal helper. Don't use directly.
#define CL_DETAIL_CONCAT_INNER(a, b) a##b
#define CL_DETAIL_CONCAT(a, b)       CL_DETAIL_CONCAT_INNER(a, b)
#define CL_DETAIL_TMP                CL_DETAIL_CONCAT(_cl_tmp_, __COUNTER__)

// CL_TRY(expr): evaluate a Result-returning expression. If Err, return it
// from the enclosing function. The result of expr is discarded.
#define CL_TRY(expr) CL_TRY_IMPL(expr, CL_DETAIL_TMP)
#define CL_TRY_IMPL(expr, tmp)                                  \
    do {                                                        \
        auto&& tmp = (expr);                                    \
        if (tmp.is_err()) [[unlikely]]                          \
            return ::cellulon::Err(std::move(tmp).unwrap_err());\
    } while (false)

// CL_TRY_OK(var, expr): bind the Ok value of expr to `var` (by reference),
// or return the Err from the enclosing function.
//
// This expands to multiple statements; cannot be used inside an if() init or
// a for() init. Use the monadic chain or a lambda IIFE in those cases.
#define CL_TRY_OK(var, expr) CL_TRY_OK_IMPL(var, expr, CL_DETAIL_TMP)
#define CL_TRY_OK_IMPL(var, expr, tmp)                          \
    auto tmp = (expr);                                          \
    if (tmp.is_err()) [[unlikely]]                              \
        return ::cellulon::Err(std::move(tmp).unwrap_err());    \
    auto& var = tmp.unwrap()
```

Caveats, listed up front so we never debug them mid-flight:

- The macros work at function scope only. `if (CL_TRY_OK(...))` does not work and never will.
- `CL_TRY_OK(x, foo())` followed by `CL_TRY_OK(x, bar())` in the same scope is a redeclaration — same as it would be in straight C++. Use different names.
- The expanded `return Err(...)` relies on the enclosing function's return type being a `Result<U, E>` where `E` accepts the unwrapped error type. Type mismatches surface as conversion errors at the `return` site, which is annoying but correct.
- For situations where `CL_TRY_OK` doesn't fit (closures with non-Result return types, member initializers), use `.and_then` or `.match`-style explicit branching instead. We have three mechanisms; this is one of them.

Usage:

```cpp
Result<Config, Error> load_sim_config(std::string_view path) {
    CL_TRY_OK(content,   read_file(path));
    CL_TRY_OK(parsed,    parse_toml(content));
    CL_TRY_OK(validated, validate_config(parsed));
    return Ok(std::move(validated));
}
```

vs the chain form:

```cpp
Result<Config, Error> load_sim_config(std::string_view path) {
    return read_file(path)
        .and_then(parse_toml)
        .and_then(validate_config);
}
```

Pick per callsite.

## How `unwrap()` integrates with `CL_ASSERT`

The whole point of rolling our own is to make `unwrap()` consistent with the project's assert philosophy. The pattern:

```cpp
// In option.hpp / result.hpp:
template<typename T>
class Option {
public:
    constexpr T& unwrap(
        std::source_location loc = std::source_location::current()
    ) & {
        if (!has_value_) [[unlikely]]
            ::cellulon::assertion_failed(
                "unwrap()", "called unwrap() on None", loc);
        return value_;
    }
    // ... and `const &`, `&&` overloads.
};
```

Two things this does that v1 got wrong:

1. The `loc` parameter is **forwarded** to `assertion_failed`. That means the assert reports the file/line of the caller of `unwrap()`, not the line inside `unwrap()` itself.
2. `assertion_failed` will need a small overload (or default-arg adjustment) so it can take a `source_location` directly, rather than always using `current()` at its own definition site. This is a one-line change to `utils.hpp`.

For `Result::unwrap()`, the assertion message should include `format(error_)` so the failure tells you *what* the error was, not just "unwrap() on Err". For typed `E` we'd need either `std::formatter<E>` or a customization point — for v1 we can require `E` to be either `Error` (which has `.format()`) or formattable. Reasonable bar.

`expect(msg)` is the same as `unwrap()` but with the caller-supplied message used in the assertion message. Same `source_location` forwarding.

## `inspect` instead of `if_some`

v1 named this `if_some`. Rust calls the equivalent method `inspect`. We use the Rust name, on both `Option` and `Result`:

```cpp
template<typename T>
class Option {
public:
    template<std::invocable<T const&> F>
    constexpr Option& inspect(F&& f) & {
        if (has_value_) std::invoke(std::forward<F>(f), value_);
        return *this;
    }
    // ... const &, &&
};
```

`inspect_err` is the symmetric one on `Result`.

## Edge cases & decisions

These are the calls that need to be made before implementing. I've taken positions on each; flip them if you disagree.

1. **Reference types (`Option<T&>`).** Supported via partial specialization, stored as `T*`. Construction from `T&`. `unwrap()` returns `T&`. No `Result<T&, E>` until needed.
2. **Move-only types.** Supported. Copy ops conditionally deleted. `Some(std::move(x))` is the explicit spelling.
3. **Move-from state.** `Option`: `take()` and `std::move(opt).unwrap()` both leave the source as `None`. `Result`: `unwrap` from rvalue moves the value out but leaves `is_ok_ = true`; subsequent `unwrap()` is UB / asserts only if you guard against double-move (we don't, document it).
4. **`noexcept`.** All internal moves are `noexcept`. Static-assert that `T` and `E` have `noexcept` move ctors. If they don't, the user fixes their type. Cellulon's components and Errors all satisfy this trivially.
5. **`constexpr`.** Aim for everything to be `constexpr`-eligible. Won't always be (anything that touches `assertion_failed` won't), but the value-typed ops (`map`, `and_then` over a constexpr `T`) should be.
6. **Equality / ordering.** `Option<T>` gets `operator==` and `operator<=>` when `T` has them, defaulted via `= default`. None < Some by convention. `Result<T, E>` gets `operator==` only; ordering on a sum type is rarely meaningful.
7. **Hashing.** Skip until we put one in a hash set. When we do, an `std::hash` specialization is fine.
8. **`format`/`print` integration.** `std::formatter<Option<T>>` and `std::formatter<Result<T, E>>` are nice-to-have. Defer until we have a real debug-print use case. `Error::format()` already exists for the common case.
9. **Allocator awareness.** No. We don't custom-allocate.
10. **Aborting on copy of `Result<T, E>` where `E = Error`.** `Error` contains a `unique_ptr<Error>`, so it's move-only. Therefore `Result<T, Error>` is move-only. This is fine and arguably correct — errors should rarely be copied. If a callsite needs to fan out the same error to multiple consumers, it can `.format()` to a string and copy that.

## Out of scope (deliberately)

Things v1 implied but we are not building:

- A full Rust-style `Iterator` adapter on `Option`/`Result`. Way too much code, wrong feature set for an ECS sim.
- `flatten`, `transpose`, `zip`, `xor` on `Option`. Add when actually needed.
- `From::from`-style automatic error conversion. C++ doesn't have it; pretending we do via implicit conversion ladders is worse than just calling `.map_err(...)`. The `CL_TRY` macros assume the error types match; mismatched ones get a compile error at the `return` site, and you fix it locally.
- `Result<T, E>::ok_or(...)` and other "convert in the wrong direction" methods.
- Try-catch interop. We don't throw, we don't catch.

## Suggested file layout

```
src/
  utils.hpp        # CL_ASSERT, CL_DBG_ASSERT, as<T>(), aliases
  option.hpp       # Option<T>, Some/None proxies, CL_TRY*
  result.hpp       # Result<T,E>, Ok/Err proxies, Error, format
```

Splitting `option.hpp` and `result.hpp` is mostly to keep each file ~150 lines. They're tightly coupled (`Option::ok_or` returns a `Result`, `Result::ok` returns an `Option`), so `result.hpp` includes `option.hpp`. `CL_TRY` macros live with `result.hpp` since they only make sense for `Result`-returning functions. (If we ever want a `CL_TRY` for `Option`, we can add `CL_TRY_SOME` later.)

`utils.hpp` already has `assertion_failed`; we just need to give it an overload that accepts a `source_location` directly so `unwrap()` can forward the caller's location. Roughly:

```cpp
[[noreturn]] inline void assertion_failed(
    const char* expression,
    std::string_view message,
    std::source_location loc
);
```

## Implementation order

When we eventually build this:

1. Extend `assertion_failed` to accept a forwarded `source_location` and `string_view` message.
2. Implement `Error` with `context()` chaining and `format()`.
3. Implement `Option<T>` (without the reference specialization).
4. Implement `Result<T, E>`.
5. Add `CL_TRY` / `CL_TRY_OK` macros.
6. Add the `Option<T&>` specialization.
7. Write a small set of tests (when the project gets a test target).

Steps 1–5 are the minimum viable version. 6–7 are quality-of-life.

## What success looks like

The day-to-day code should look like Rust with C++ punctuation. Short, declarative, no exceptions, no nullable raw pointers as error signals. `unwrap()` panics loudly with a useful message and the right source location. `CL_TRY_OK` handles the boring imperative cases. `.and_then`/`.map` handle the pipeline cases. `match`-style explicit branching is available when neither fits. When a stdlib quirk comes up, it's not in our error-handling code, because our error-handling code doesn't use the stdlib types it's reinventing.

If after a month of writing simulation code we find ourselves reaching past these types for `std::optional` or raw return-codes or out-parameters, the API design failed and we revisit.
