-- cellulon: genetic natural selection simulation
set_project("cellulon")
set_version("0.0.1")
set_languages("c++23")

-- Use clang everywhere. On Windows this will pick clang++; if you later decide
-- you want the MSVC-ABI flavor, switch this to "clang-cl".
set_toolchains("clang")

-- Sensible warnings and debug info for the default build.
set_warnings("all", "extra", "pedantic")
add_cxflags("-Wno-unused-parameter", {tools = {"clang", "gcc"}})

-- Build modes: `xmake f -m debug` or `xmake f -m release`
add_rules("mode.debug", "mode.release")

-- Auto-regenerate compile_commands.json at the project root after each build
-- so clangd always has up-to-date include paths.
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

-- Dependencies. xmake will fetch and build these on first configure.
add_requires("raylib 5.5.x", {configs = {shared = false}})
add_requires("entt 3.14.x")

target("cellulon")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("raylib", "entt")

    -- NOTE: uncomment if sanitization needed.
    -- if is_mode("debug") then
    --     add_cxxflags("-fno-omit-frame-pointer", "-fsanitize=address,undefined")
    --     add_ldflags("-fsanitize=address,undefined", {force = true})
    -- end

    -- On macOS raylib needs these frameworks; xmake's raylib package
    -- usually pulls them automatically, but being explicit is harmless.
    if is_plat("macosx") then
        add_frameworks("Cocoa", "IOKit", "CoreVideo", "OpenGL")
    end

    -- Use libc++ everywhere we're using Clang (Linux, Mac, Windows/MSYS2)
    if is_plat("linux", "macosx", "mingw") then
        add_cxxflags("-stdlib=libc++", {tools = {"clang"}})
        add_ldflags("-stdlib=libc++", {tools = {"clang"}, force = true})
        if is_plat("linux") then
            add_syslinks("c++abi")
        end
    end

target("celltest")
    set_kind("binary")
    add_includedirs("src", "celltest")
    add_files("celltest/*.cpp")
    add_defines("CELLTEST")

    if is_mode("debug") then
        add_cxxflags("-fno-omit-frame-pointer",
                     "-fsanitize=address,undefined")
        add_ldflags("-fsanitize=address,undefined", {force = true})
    end

    -- Clang has exceptions on by default. This is just to document that exceptions are only deliberately used in testing, but not in the main executable.
    add_cxxflags("-fexceptions", {tools = {"clang"}})

    if is_plat("linux", "macosx", "mingw") then
        add_cxxflags("-stdlib=libc++", {tools = {"clang"}})
        add_ldflags("-stdlib=libc++", {tools = {"clang"}, force = true})
        if is_plat("linux") then
            add_syslinks("c++abi")
        end
    end

