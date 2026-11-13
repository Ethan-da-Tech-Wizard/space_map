# SpaceMap

SpaceMap is a C++ and Qt desktop project for exploring disk usage. It brings directory scanning, folder summaries, and a treemap view together in one interface.

**Status:** Desktop utility; current builds and releases have not been independently verified in this review.

## Explore the project

- Scan a directory and inspect the resulting size hierarchy and visual treemap.
- Source includes a parallel scanning coordinator and platform-specific filesystem code.
- The project also documents file-management actions. Review an action carefully before changing or deleting files.

## Build from source

You need a C++20 compiler, CMake, and Qt 6 Widgets. From the repository root:

```sh
cmake -S . -B build
cmake --build build
```

Platform-specific prerequisites, launch instructions, and historical release information are in [PROJECT_NOTES.md](PROJECT_NOTES.md). These notes include older release and security-related instructions; verify their applicability to your system before following them. We have not independently tested installation or release packages in this review.

## More information

[CMake build definition](CMakeLists.txt) · [Source](src/) · [Project guide](docs/PROJECT_GUIDE.md) · [License review](docs/LICENSE_REVIEW.md)

## License

Project code is distributed under the [MIT License](LICENSE). Dependencies and packaged assets may have their own terms; see the [license review](docs/LICENSE_REVIEW.md) and [dependency inventory](docs/DEPENDENCIES.csv).
