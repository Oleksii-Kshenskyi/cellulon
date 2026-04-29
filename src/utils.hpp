#ifndef _UTILS_HPP_
#define _UTILS_HPP_
// The idea of this file is - it's a collection of seemingly random functions/classes/helpers that are going to be used across the Cellulon codebase.
// What makes it slightly different from your usual utils headers is - it is specifically designed to make C++ less "painful" for me. It can use some seemingly "hacky" or "unidiomatic" solutions, if so that would be by design - the intention is not to write hyper-optimized code, but instead write code in a way that's compatible with and understandable for my brain (and less annoying than default C++).

#include <cstdint>
#include <random>
#include <type_traits>
#include <concepts>
#include <cstdlib>
#include <source_location>
#include <print>

/// Synonym of static_cast. Makes static_cast less annoying to write out and approaches how conversions are done in several other languages.
template<typename T, typename U>
constexpr T as(U value) { return static_cast<T>(value); }

// Number type aliases. Yeah yeah, nonstandard, blah blah. I know, don't care. Keeps me sane.
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

// IDEA: at some point, potentially, implement logging / OR integrate Imgui for greater debug visibility into what's happening in the simulation.

namespace cellulon {
    [[noreturn]] inline void assertion_failed(
        const char* expression,
        const char* message,
        std::source_location loc = std::source_location::current()
    ) {
        std::print("\n[ASSERTION FAILED] WHERE? => {}:{} in function {}:\n",
            loc.file_name(),
            loc.line(),
            loc.function_name());
        std::print("    WHAT? => `{}`;\n", expression);
        std::print("    WHY? => `{}`.\n\n", message);
        std::abort();
    }
}

#define CL_ASSERT(expr, msg) \
    do {if(!(expr)) [[unlikely]] \
        ::cellulon::assertion_failed(#expr, msg); \
    } while(false)

#ifdef NDEBUG
    #define CL_DBG_ASSERT(expr, msg) do {} while(false);
#else
    #define CL_DBG_ASSERT(expr, msg) CL_ASSERT(expr, msg);
#endif

// FIXME: does GameRNG implementation have all I need for randomization of the first simulation MVP?
namespace cellulon::random {
    class GameRNG {
        public:
            explicit GameRNG(u64 seed): engine(seed) {} // construct from deterministic seed
            GameRNG(): engine(std::random_device{}()) {} // construct from entropy

            template<std::integral Ti>
            Ti range(Ti min, Ti max) {
                std::uniform_int_distribution<Ti> dist {min, max};
                return dist(this->engine);
            }

            template<std::floating_point Tf>
            Tf range(Tf min, Tf max) {
                std::uniform_real_distribution<Tf> dist {min, max};
                return dist(this->engine);
            }
        private:
            std::mt19937_64 engine;
    };
}

namespace cellulon {
    struct NoneType { explicit constexpr NoneType() = default; };
    constexpr inline NoneType None {};

    template <typename T>
    struct SomeType {
        T value;
    };

    template <typename T>
    constexpr SomeType<std::decay_t<T>> Some(T&& new_val) {
        return SomeType<std::decay_t<T>> { std::forward<T>(new_val) };
    }

    // TODO: Copy/move constructors
    // TODO: assignments from Some and Option
    // TODO: at least a basic .map() + .and_then()
    template <typename T>
    class Option {
        public:
            constexpr Option<T>& reset_to_none() {
                if(this->_is_some) {
                    this->_is_some = false;
                    std::destroy_at(&this->value);
                }
                this->no_value = 0;

                return *this;
            }

            constexpr Option(NoneType n) noexcept: _is_some(false) {}
            constexpr Option() noexcept: _is_some(false) {}

            template<typename U>
                requires std::constructible_from<T, U&&>
            constexpr Option(SomeType<U> some_v): _is_some(true) {
                std::construct_at(&this->value, std::move(some_v.value));
            }

            constexpr Option<T>& operator=(NoneType n) noexcept {
                this->reset_to_none();
                return *this;
            }

            constexpr bool is_some() {
                return this->_is_some;
            }
            constexpr bool is_none() {
                return !this->_is_some;
            }

            constexpr ~Option() requires std::is_trivially_destructible_v<T> = default;
            constexpr ~Option() {
                if(this->_is_some) {
                    this->_is_some = false;
                    std::destroy_at(&this->value);
                }
            }
        private:
            union {
                T value;
                char no_value {};
            };
            bool _is_some;
    };
}

// TODO: [[!!]] Develop an Option/Result alternative to use for error handling in Cellulon.

// TODO: Implement mini-test framework according to knowledge/on_testing.md.

#endif