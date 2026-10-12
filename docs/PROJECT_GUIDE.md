# SpaceMap

A native disk-space explorer with parallel scanning, directory summaries, and treemap views.

**Status:** Desktop utility  
**Built with:** C++20, Qt 6, CMake

## Reading order

1. Read the root README for project purpose and setup navigation.
2. Follow the entry points below to find the implementation.
3. Use the file guide to locate individual modules, tests, assets, and configuration.
4. Check component licenses before redistributing dependencies or assets.

## Important paths

- [CMakeLists.txt](../CMakeLists.txt) — CMake build definition for .; declares targets, dependency discovery, and compiler/build settings.
- [src](../src) — Application source and platform integration
- [PROJECT_NOTES.md](../PROJECT_NOTES.md) — Preserved detailed setup, usage, architecture, and development notes from the earlier README.

## Structure

| Path | Role |
| --- | --- |
| [.github/](../.github/) | GitHub workflow and contributor configuration (1 tracked entries). |
| [docs/](../docs/) | Project documentation, decisions, and reference material (6 tracked entries). |
| [src/](../src/) | Application source and platform integration (16 tracked entries). |
| [super_indepth_code_explainer/](../super_indepth_code_explainer/) | Project-specific super indepth code explainer components (6 tracked entries). |

## Useful commands

Run from the repository root after following the documented prerequisites.

```sh
cmake -S . -B build
cmake --build build
```

## Scope

Install the Qt components listed by CMake before building. Review deletion actions before using them on valuable data. Existing release packaging requires its own dependency notices.

## Maintaining the guide

The file catalog is a source-inspection snapshot, not proof that every runtime path works. Update it when moving files or changing module responsibilities. The 2026-10-12 documentation review did not run every application, deploy services, or validate all historical performance claims.
