# Cellulon

Take 2 at a genetics simulation, this time in C++/Raylib. For now, most of the details are TBD, this is just as much a second attempt at a genetics simulation as it is an attempt for me to reconcile with C++ and see if I can seriously use it for my personal projects.

The simulation is going to be 2D (probably a turn-based grid), not generation-based (continuous), the agents will have a chance to reproduce once they've fulfilled certain conditions, and we'll see what comes of it.

Everything else is TBD.

## Building it

Building Cellulon requires:
- xmake (build system);
- clang (compiler) - Clang ONLY, with libc++ (Clang's own C++ stdlib implementation). Should work just fine on Linux/Mac;
- On Windows, requires MSYS2/CLANG64. Should build there normally in the future.

Main development system is CachyOS/Wayland (Linux). MacOS/Windows should work eventually, but they're secondary priorities for now.
Would very much like to do native Wayland, but raylib's underlying GLFW backend still doesn't support Wayland properly and xmake xrepo's package only has the X11 version of Raylib. Hopefully in the future. For now, XWayland it is.

Once you have everything:

```sh
xmake f -m debug -y # configure debug build 
xmake f -m release -y # or alternatively, configure release build
xmake # build it
xmake run cellulon # run it

xmake run celltest # testing cellulon; doesn't exist for now. should be available in the near future.
```

## Thirdparty C++ libs

For now, the project only uses Raylib (graphics/rendering) and EnTT (ECS). Can change in the future.
