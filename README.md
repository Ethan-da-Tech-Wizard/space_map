# SpaceMap

A native disk-space explorer with parallel scanning, directory summaries, and treemap views.

**Status:** Desktop utility  
**Built with:** C++20, Qt 6, CMake

[Project guide](docs/PROJECT_GUIDE.md) · [Every file explained](docs/FILE_GUIDE.md) · [Licenses and dependencies](docs/LICENSE_REVIEW.md)

## Start here

- [CMakeLists.txt](CMakeLists.txt) — CMake build definition for .; declares targets, dependency discovery, and compiler/build settings.
- [src](src) — Application source and platform integration
- [PROJECT_NOTES.md](PROJECT_NOTES.md) — Preserved detailed setup, usage, architecture, and development notes from the earlier README.

The following commands are taken from this project’s source/configuration. Install its prerequisites first.

```sh
cmake -S . -B build
cmake --build build
```

[Detailed setup, usage, and development notes](PROJECT_NOTES.md) preserve the previous project documentation.

## Repository layout

| Path | Role |
| --- | --- |
| [.github/](.github/) | GitHub workflow and contributor configuration (1 tracked entries). |
| [docs/](docs/) | Project documentation, decisions, and reference material (6 tracked entries). |
| [src/](src/) | Application source and platform integration (16 tracked entries). |
| [super_indepth_code_explainer/](super_indepth_code_explainer/) | Project-specific super indepth code explainer components (6 tracked entries). |

## Scope and current limits

Install the Qt components listed by CMake before building. Review deletion actions before using them on valuable data. Existing release packaging requires its own dependency notices.

## License

Original project code retains **MIT** terms in [LICENSE](LICENSE). Third-party libraries, media, data, models, and tools retain their own terms. See the [license review](docs/LICENSE_REVIEW.md) and [dependency inventory](docs/DEPENDENCIES.csv).
