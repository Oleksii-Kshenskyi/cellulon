# `unwrap()` ref-qualification and move semantics

This doc captures the design decision and mechanical explanation behind `unwrap()` on rvalue `Option<T>` / `Result<T, E>`. It follows from `on_error_adts_v2.md` and resolves the one specific thing that was blocking a final implementation.

## The problem

The most common use of `Option` is:

```cpp
Option<T> get_something();

auto val = get_something().unwrap();
```

If `unwrap()` returns `T&`, the reference is bound to the inside of the temporary `Option`. The temporary is destroyed at the end of the full expression. `val` dangles. This isn't a theoretical concern — it's immediate UB in the most natural call pattern.

The three apparent options were:

1. **Return `T&` and accept the dangling** — obviously wrong.
2. **`= delete` the `&&` overload** — prevents the use case entirely, defeats the purpose.
3. **Return by copy** — works, but a performance regression for move-only and heap-owning types.
4. **Return `T` by move from the `&&` overload** — the correct answer, listed fourth because it took a while to see.

## The solution: ref-qualified overloads with different return types

`unwrap()` gets three overloads, distinguished by the ref-qualification of `*this`:

```cpp
template<typename T>
class Option {
public:
    constexpr T& unwrap(
        std::source_location loc = std::source_location::current()
    ) & {
        if (!this->has_value_) [[unlikely]]
            ::cellulon::assertion_failed("unwrap()", "called unwrap() on None", loc);
        return this->value_;
    }

    constexpr const T& unwrap(
        std::source_location loc = std::source_location::current()
    ) const & {
        if (!this->has_value_) [[unlikely]]
            ::cellulon::assertion_failed("unwrap()", "called unwrap() on None", loc);
        return this->value_;
    }

    constexpr T unwrap(
        std::source_location loc = std::source_location::current()
    ) && {
        if (!this->has_value_) [[unlikely]]
            ::cellulon::assertion_failed("unwrap()", "called unwrap() on None", loc);
        return std::move(this->value_);
    }
};
```

The `&&` overload returns `T` by value. When `*this` is an rvalue, the `Option` is dying regardless — so move the `T` out and hand it to the caller by value. The caller owns it. No dangling reference.

The pattern works because:

```cpp
auto cfg = load_config().unwrap();    // calls unwrap() &&, returns T by move, cfg owns it
auto& cfg = load_config().unwrap();   // COMPILE ERROR — auto& can't bind to a prvalue
auto&& cfg = load_config().unwrap();  // Fine — lifetime extension applies to the materialized T
```

The compiler itself prevents the dangling-reference mistake. `auto&` can't bind to the prvalue returned by the `&&` overload.

This is also exactly what `std::optional::value()` and `std::expected::value()` do. (The stdlib returns `T&&` rather than `T` for the `&&` overload; returning plain `T` is the more conservative choice and gives cleaner prvalue semantics — the move is forced immediately rather than deferred.)

## How `std::move` produces a move

`std::move(this->value_)` does not move anything. It casts `this->value_` (an lvalue) to `T&&` (an xvalue). Zero bytes move. The cast is a no-op at runtime.

The actual move happens at the `return` statement, and the mechanism is overload resolution on `T`'s constructor:

1. `std::move(this->value_)` produces an expression of type `T&&`.
2. The `return` statement must construct the return value (a `T`) from that expression.
3. The compiler considers two candidates: `T(const T&)` (copy) and `T(T&&)` (move).
4. `T&&` binds better to `T&&` than to `const T&`. Overload resolution picks the move constructor.
5. The move constructor runs. This is where the actual data transfer happens.

The "no copy" guarantee lives in this overload resolution. `std::move` is the mechanism that makes the compiler pick the move constructor over the copy constructor. Nothing more.

The guarantee can be made compile-time-visible with:

```cpp
static_assert(std::is_move_constructible_v<T>,
    "Option<T> requires T to be move-constructible");
```

This is consistent with the decision in `on_error_adts_v2.md` to `static_assert` `noexcept` move ctors. With move-constructibility asserted, `return std::move(this->value_)` provably calls a move constructor and never silently falls back to copy.

Note: NRVO does not apply here. NRVO requires returning a local variable by name; `this->value_` is a member of the dying `Option`. The move constructor will run.

## What "move" actually means at the machine level

For types whose entire state is inline on the stack — `Vec3`, `entt::entity`, `Cell`, `i32` — a move and a copy produce identical machine code. Both copy the same N bytes to the return location. The `std::move` accomplished nothing observable for these types.

Move is only meaningfully different from copy when `T` **owns a resource through an indirection** — a heap allocation, a file descriptor, a GPU handle. The canonical example is `std::string`:

```
struct string {
    char* ptr;       // on stack, points to heap buffer
    size_t size;
    size_t capacity;
};
```

Both copy and move copy the 24 bytes of the struct. The difference:

- **Copy constructor**: copies 24 bytes, then allocates new heap memory and memcpys the character data. O(n).
- **Move constructor**: copies 24 bytes, then nulls out `ptr` in the source so its destructor won't free the buffer. O(1).

The "move" is never about avoiding the copy of the struct itself. It's about avoiding the duplication of whatever heap resource the struct owns.

For Cellulon's typical component types (small structs of ints, floats, enums), there's nothing to steal. A move is a copy. This is fine — it's still O(1), it's still fast, and the ref-qualified `&&` overload is still necessary and correct for a different reason entirely.

## Why `unwrap() &&` matters even for trivial types

The purpose of the `&&` overload returning `T` by value is not primarily about avoiding a copy. It's about **giving the caller a correctly-lifetimed value**.

`T&` would dangle. `T` doesn't. For types with heap state, you also avoid an expensive allocation. For trivial types, the distinction is just correctness: you get an owned `T` that lives as long as the caller needs it, rather than a reference into a dead temporary. Same cost either way; unambiguously correct rather than UB.

## `expect()` and the rest of the extraction API

Every method that extracts the inner value should use the same ref-qualification pattern:

- `expect(msg)` — same three overloads as `unwrap()`, with a caller-supplied message.
- `unwrap_or(d)` — already returns `T` by value in all overloads; no issue.
- `unwrap_or_else(f)` — same.

`take()` — the mutable named-lvalue version that leaves the source as `None` — is a separate operation and only makes sense on `&` (no `&&` overload needed; calling `take()` on a temporary is a no-op anyway since the temporary destructs immediately).
