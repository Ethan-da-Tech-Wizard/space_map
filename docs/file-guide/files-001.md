# Files 1 of 1 — SpaceMap

[All pages](../FILE_GUIDE.md) · [Project guide](../PROJECT_GUIDE.md)

| File | Role and useful details |
| --- | --- |
| [.github/workflows/release.yml](../../.github/workflows/release.yml) | GitHub Actions workflow for release; defines automation triggers, jobs, and build/check steps. |
| [.gitignore](../../.gitignore) | Defines local data, credentials, dependencies, and generated outputs that Git should leave untracked. |
| [CMakeLists.txt](../../CMakeLists.txt) | CMake build definition for .; declares targets, dependency discovery, and compiler/build settings. |
| [LICENSE](../../LICENSE) | Preserved license, copyright, or attribution terms; read the full text before reusing the covered component. |
| [PROJECT_NOTES.md](../../PROJECT_NOTES.md) | Preserved detailed setup, usage, architecture, and development notes from the earlier README. |
| [README.md](../../README.md) | Project overview, current status, entry points, setup navigation, and documentation links. |
| [docs/DEPENDENCIES.csv](../../docs/DEPENDENCIES.csv) | Machine-readable inventory of declared and locked dependencies, versions, reported licenses, and evidence URLs. |
| [docs/FILE_GUIDE.md](../../docs/FILE_GUIDE.md) | Index of the complete tracked-file guide, with one entry for every file and links to the paginated descriptions. |
| [docs/LICENSE_REVIEW.md](../../docs/LICENSE_REVIEW.md) | Original-code license, preserved component notices, dependency-license evidence, and unresolved distribution questions. |
| [docs/PROJECT_GUIDE.md](../../docs/PROJECT_GUIDE.md) | Maintainer orientation: purpose, important entry points, source layout, commands, and project-specific boundaries. |
| [docs/file-guide/files-001.md](../../docs/file-guide/files-001.md) | One page of the tracked-file catalog; links each listed path to its role and source-derived details. |
| [docs/spacemap-scan.png](../../docs/spacemap-scan.png) | Image or interface asset for spacemap scan; 1024 × 768 pixels. Source/asset rights remain separate from software-license terms. |
| [io.github.spacemap.desktop](../../io.github.spacemap.desktop) | Linux desktop launcher for io.github.spacemap; records the command, icon, and desktop integration settings. |
| [src/file_tree_model.cpp](../../src/file_tree_model.cpp) | Source module for file tree model; defines `QAbstractItemModel`, `FileTreeModel::setRootNode`, `FileTreeModel::index`, `FileTreeModel::parent`, `FileTreeModel::rowCount`, `FileTreeModel::columnCount`, `FileTreeModel::data` and 6 more symbols. |
| [src/file_tree_model.hpp](../../src/file_tree_model.hpp) | Source module for file tree model; defines `FileTreeModel`, `QModelIndex`, `PercentDelegate`. |
| [src/main.cpp](../../src/main.cpp) | Source module for main; defines `rusage`, `main`. |
| [src/main_window.cpp](../../src/main_window.cpp) | Source module for main window; defines `MainWindow::setupUi`, `connect`, `QTimer::singleShot`, `MainWindow::applyDarkTheme`, `MainWindow::updateControls`, `MainWindow::updateStatsHUD`, `MainWindow::onBrowseClicked` and 16 more symbols. |
| [src/main_window.hpp](../../src/main_window.hpp) | Source module for main window; defines `MainWindow`. |
| [src/platform/scanner_linux.cpp](../../src/platform/scanner_linux.cpp) | Source module for scanner linux; defines `statx_timestamp`, `statx`, `dirent`, `stat`. |
| [src/platform/scanner_mac.cpp](../../src/platform/scanner_mac.cpp) | Source module for scanner mac; defines `dirent`, `stat`. |
| [src/platform/scanner_win.cpp](../../src/platform/scanner_win.cpp) | Source module for scanner win; defines `utf8_to_utf16`, `utf16_to_utf8`. |
| [src/scanner.cpp](../../src/scanner.cpp) | Source module for scanner; defines `statvfs`, `stat`, `Scanner::reset`, `Scanner::start`, `Scanner::pause`, `Scanner::resume`, `Scanner::cancel` and 5 more symbols. |
| [src/scanner.hpp](../../src/scanner.hpp) | Source module for scanner; defines `Scanner`, `DirId`, `Task`, `running`, `paused`, `cancelled`, `files_scanned` and 7 more symbols. |
| [src/theme_dialog.cpp](../../src/theme_dialog.cpp) | Source module for theme dialog; defines `ThemeDialog::setupUi`, `connect`, `ThemeDialog::applyDialogStyle`, `ThemeDialog::rebuildPresetGrid`, `QMetaObject::invokeMethod`, `ThemeDialog::createPresetCard`, `ThemeDialog::addColorSection` and 11 more symbols. |
| [src/theme_dialog.hpp](../../src/theme_dialog.hpp) | Source module for theme dialog; defines `ThemeDialog`. |
| [src/theme_manager.cpp](../../src/theme_manager.cpp) | Source module for theme manager; defines `ThemeManager::instance`, `ThemeManager::builtinPresets`, `ThemeManager::userPresets`, `ThemeManager::activeTheme`, `ThemeManager::activePresetIndex`, `ThemeManager::isActiveUserPreset`, `ThemeManager::applyBuiltinPreset` and 11 more symbols. |
| [src/theme_manager.hpp](../../src/theme_manager.hpp) | Source module for theme manager; defines `Theme`, `ThemeManager`. |
| [src/tree_node.cpp](../../src/tree_node.cpp) | Source module for tree node; defines `name`, `TreeNode::find_child`, `TreeNode::get_or_create_child`, `TreeNode::propagate_stats`, `TreeNode::sort_children_by_size`, `TreeNode::test_tree_node`. |
| [src/tree_node.hpp](../../src/tree_node.hpp) | Source module for tree node; defines `TreeNode`. |
| [super_indepth_code_explainer/cmake_and_main.md](../../super_indepth_code_explainer/cmake_and_main.md) | Documentation: SpaceMap Architecture Book: Chapters 1 & 2. Covers CHAPTER 1: The Build System & Security Configurations (CMakeLists.txt); CHAPTER 2: The Application Entrypoint & Elevation (src/main.cpp). |
| [super_indepth_code_explainer/data_model.md](../../super_indepth_code_explainer/data_model.md) | Documentation: SpaceMap Architecture Book: Chapter 6. Covers CHAPTER 6: Model-View-Controller & Custom Canvas Drawing; 1. src/file_tree_model.hpp (Header File Breakdown); 2. src/file_tree_model.cpp (Source File Breakdown). |
| [super_indepth_code_explainer/main_window.md](../../super_indepth_code_explainer/main_window.md) | Documentation: SpaceMap Architecture Book: Chapter 7. Covers CHAPTER 7: Graphical User Interface & File Recycling Engine; 1. src/main_window.hpp (Header File Breakdown); 2. src/main_window.cpp (Source File Breakdown). |
| [super_indepth_code_explainer/platform_scanners.md](../../super_indepth_code_explainer/platform_scanners.md) | Documentation: SpaceMap Architecture Book: Chapter 5. Covers CHAPTER 5: Platform-Specific Filesystem Walkers; 1. src/platform/scanner_linux.cpp (Linux Platform Walker); 2. src/platform/scanner_win.cpp (Windows Platform Walker). |
| [super_indepth_code_explainer/scanner.md](../../super_indepth_code_explainer/scanner.md) | Documentation: SpaceMap Architecture Book: Chapter 4. Covers CHAPTER 4: Multi-Threaded Scanning Engine & Task Synchronization; 1. src/scanner.hpp (Header File Breakdown); 2. src/scanner.cpp (Source File Breakdown). |
| [super_indepth_code_explainer/tree_node.md](../../super_indepth_code_explainer/tree_node.md) | Documentation: SpaceMap Architecture Book: Chapter 3. Covers CHAPTER 3: Memory-Optimized Tree Nodes & Thread Safety; 1. src/tree_node.hpp (Header File Breakdown); 2. src/tree_node.cpp (Source File Breakdown). |
