# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Cellulon is a 2D continuous (not generation-based) genetic natural selection simulation built with C++23, Raylib 5.5, and EnTT 3.14. The window starts maximized and non-resizable. The game loop runs at 30 FPS.

## Build System

The project uses [XMake](https://xmake.io). Dependencies (raylib, entt) are fetched automatically on first configure.

```sh
xmake                        # build (debug by default)
xmake f -m release && xmake  # release build
xmake run                    # build and run
xmake f -m debug && xmake    # switch back to debug
```

There are no tests yet.

## Toolchain

- Compiler: clang (C++23), set via `set_toolchains("clang")` in xmake.lua
- LSP: clangd with clang-tidy enabled; `compile_commands.json` is auto-generated at the project root after each build
- IntelliSense from ms-vscode.cpptools is disabled — clangd handles everything
- Debugging: CodeLLDB (F5 in VS Code)

## Architecture

All global constants live in `src/constants.hpp` under the `cellulon::constants` namespace (aliased as `cnst` in main). New constants go there.

The ECS registry (`entt::registry`) is created in `main()` and will be the central store for all simulation entities and components. There is no game-object hierarchy — everything is data in the registry.

New source files drop into `src/` and are picked up automatically by `add_files("src/*.cpp")`.

This development environment is VS Code + XMake + Clang + Linux/KWin Wayland.

src/main.cpp is the entry point.

NEVER make any changes without EXPLICITLY confirming with the user whether they're necessary. ALWAYS assume by default that the user is asking a question unless explicitly stated "please implement X", "write a function to X", etc.

## Commitments

- NEVER use ANY exceptions explicitly in the code;
- Cast with as<T>() (alias to static_cast);
- We use Rust-like int/float type aliases instead of _t or plain numeric type names, as per src/utils.hpp.
- Error handling done with the custom Result/Option classes in src/utils.hpp.
- Aggressive assert policy via CL_ASSERT / CL_DBG_ASSERT in src/utils.hpp.
- ALL non-static class members (BOTH methods and fields) are called via this->method() or this->var. No exceptions.
