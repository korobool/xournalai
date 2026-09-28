# <img src="ui/pixmaps/com.github.xournalpp.xournalpp.svg" align="left" width="100" height="100">  <br> xournalai

> [!WARNING]
> **This is an experimental fork of [Xournal++](https://github.com/xournalpp/xournalpp) for personal use.**
> It is not affiliated with or supported by the Xournal++ team. Things may be broken, unfinished, or change without notice.
> If you just want a stable note-taking app, use the [original project](https://github.com/xournalpp/xournalpp) and its
> [official releases](https://github.com/xournalpp/xournalpp/releases).

<img src="readme/main.png" width=550px title="Xournal++ on GNU/Linux"/>

## About

Xournal++ is a C++ note-taking application for handwritten notes. It supports pressure-sensitive styluses, lets you annotate PDFs,
draws shapes, renders LaTeX, records audio alongside your notes, and runs Lua plugins. This fork is where I try out my own changes
on top of that codebase.

## Building

Xournal++ uses CMake and Ninja and needs a C++20 compiler.

```sh
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_INSTALL_PREFIX=install
cmake --build . --target install
./install/bin/xournalpp
```

The build instructions for each platform, including the dependency lists, are in [LinuxBuild.md](readme/LinuxBuild.md),
[MacBuild.md](readme/MacBuild.md) and [WindowsBuild.md](readme/WindowsBuild.md). Instructions for building and running the
tests are in [Compile.md](readme/Compile.md).

## Original project

All credit for Xournal++ goes to its authors and contributors (see [AUTHORS](AUTHORS)).

- Repository: https://github.com/xournalpp/xournalpp
- Website & user guide: https://xournalpp.github.io
- Report upstream bugs and contribute there, not here.

## License

GNU GPL v2 or later, same as upstream. See [LICENSE](LICENSE).
