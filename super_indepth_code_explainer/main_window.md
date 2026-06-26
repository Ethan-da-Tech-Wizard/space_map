# SpaceMap Architecture Book: Chapter 7

This chapter provides a complete line-by-line, character-by-character architectural explanation of the Graphical User Interface Window (`src/main_window.hpp` and `src/main_window.cpp`).

---

# CHAPTER 7: Graphical User Interface & File Recycling Engine

The user interface of SpaceMap is written in native C++ using **Qt6 widgets**. It coordinates the scanning engine, updates data dashboards, handles user interactions, and provides a safe file deletion tool. It is styled with custom Qt Style Sheets (QSS) for a modern dark-mode aesthetic with smooth interactions. It features dynamic Linux block device discovery and auto-mounting.

---

## 1. src/main_window.hpp (Header File Breakdown)

The header defines the window layout containing search controls, loading bars, tree grids, and statistics HUDs.

```cpp
1: #pragma once
```
* **Explanation**: Preprocessor header guard.

```cpp
3: #include <QMainWindow>
4: #include <QLineEdit>
5: #include <QPushButton>
6: #include <QLabel>
7: #include <QTreeView>
8: #include <QTimer>
9: #include <QSplitter>
10: #include <QProgressBar>
11: #include <QComboBox>
12: #include <QStorageInfo>
13: #include <QJsonArray>
14: #include <QJsonObject>
15: #include "scanner.hpp"
16: #include "file_tree_model.hpp"
```
* **Explanation**: Imports Qt layout elements and classes:
  * `<QMainWindow>`: Provides main application window frame.
  * `<QLineEdit>`: Directory path text field.
  * `<QPushButton>`: Buttons for interaction.
  * `<QLabel>`: Text displays.
  * `<QTreeView>`: Hierarchical grid to show directories.
  * `<QTimer>`: Triggers periodic GUI refreshes.
  * `<QProgressBar>`: Visual scan progress.
  * `<QComboBox>`: Dropdown selection box for available drives.
  * `<QStorageInfo>`: Fallback cross-platform volume discovery.
  * `<QJsonArray>` & `<QJsonObject>`: Handles JSON parsing of disk structure.
  * `"scanner.hpp"` & `"file_tree_model.hpp"`: Scanner and Tree components.

```cpp
18: class MainWindow : public QMainWindow {
19:     Q_OBJECT
```
* **Explanation**: Defines the `MainWindow` class with the `Q_OBJECT` macro, enabling Qt's signals and slots mechanism.

```cpp
20: public:
21:     explicit MainWindow(QWidget* parent = nullptr);
22:     ~MainWindow() override = default;
```
* **Explanation**: Constructor (accepts parent widget for lifecycle management) and destructor.

```cpp
24: private slots:
25:     void onBrowseClicked();
26:     void onScanClicked();
27:     void onPauseClicked();
28:     void onCancelClicked();
29:     void onScanFinished();
30:     void onUpdateTimer();
31:     void onDeleteToggleToggled(bool checked);
32:     void onDeleteActionTriggered();
33:     void onCustomContextMenuRequested(const QPoint& pos);
34:     void onDriveSelected(int index);
35:     void refreshDrivesList();
```
* **Explanation**: Private Qt slots mapped to user actions.
  * `onDriveSelected`: Triggered when selecting a drive.
  * `refreshDrivesList`: Queries system block devices.

```cpp
37: private:
38:     void setupUi();
39:     void applyDarkTheme();
40:     void updateControls();
41:     void updateStatsHUD(uint64_t files, uint64_t dirs, uint64_t bytes);
42:     QString getAbsoluteNodePath(TreeNode* node);
43:     void propagate_deletion_stats(TreeNode* start_node, uint64_t size_removed, uint64_t allocated_removed, uint64_t files_removed, uint64_t dirs_removed);
44:     void parseBlockDevices(const QJsonArray& devices);
45:     void processPartition(const QJsonObject& partObj);
```
* **Explanation**: Internal helper methods.
  * `parseBlockDevices` & `processPartition`: Traverse block device arrays from `lsblk`.

```cpp
47:     // Scanner & Data Model
48:     Scanner* m_scanner = nullptr;
49:     FileTreeModel* m_treeModel = nullptr;
50: 
51:     // UI Widgets
52:     QComboBox* m_driveComboBox = nullptr;
53:     QPushButton* m_refreshDrivesBtn = nullptr;
54:     QLineEdit* m_pathEdit = nullptr;
55:     QPushButton* m_browseBtn = nullptr;
56:     QPushButton* m_scanBtn = nullptr;
57:     QPushButton* m_pauseBtn = nullptr;
58:     QPushButton* m_cancelBtn = nullptr;
59:     QPushButton* m_deleteToggleBtn = nullptr;
60:     QAction* m_deleteAction = nullptr;
```
* **Explanation**: Declares private pointers to all member widgets and models.

```cpp
63:     // HUD Stats
64:     QLabel* m_statusLabel = nullptr;
65:     QLabel* m_filesLabel = nullptr;
66:     QLabel* m_dirsLabel = nullptr;
67:     QLabel* m_bytesLabel = nullptr;
68:     QLabel* m_freeLabel = nullptr;
```
* **Explanation**: HUD labels tracking files, folders, size, and free space.

```cpp
71:     // UI View Component
72:     QTreeView* m_treeView = nullptr;
73:     QProgressBar* m_progressBar = nullptr;
74: 
75:     // GUI Refresh Timer
76:     QTimer* m_guiTimer = nullptr;
77: };
```
* **Explanation**: Declares private pointers to layout components and GUI refresh timer.

---

## 2. src/main_window.cpp (Source File Breakdown)

The source file handles layout construction, stylesheets, block-device discovery, partition mounting, and directory tree interactions.

```cpp
1: #include "main_window.hpp"
...
15: #include <QJsonDocument>
16: #include <QProcess>
```
* **Explanation**: Includes standard C++ headers and Qt core headers, including JSON handling and process spawning.

```cpp
19: MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
20:     m_scanner = new Scanner(this);
21:     m_treeModel = new FileTreeModel(nullptr, this);
22: 
23:     m_guiTimer = new QTimer(this);
24:     connect(m_guiTimer, &QTimer::timeout, this, &MainWindow::onUpdateTimer);
25: 
26:     setMinimumSize(800, 600);
27:     setupUi();
28:     applyDarkTheme();
29:     updateControls();
30: }
```
* **Explanation**: Constructor. Binds components, starts refresh timers, sets a minimum window dimension constraint (`800x600`), and initiates setup.

```cpp
31: void MainWindow::setupUi() {
32:     setWindowTitle("SpaceMap - C++ High-Speed Disk Analyzer");
33: 
34:     QWidget* centralWidget = new QWidget(this);
35:     setCentralWidget(centralWidget);
36: 
37:     QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
38:     mainLayout->setContentsMargins(10, 10, 10, 10);
39:     mainLayout->setSpacing(10);
```
* **Explanation**: Sets the main window properties and instantiates a vertical layout container.

```cpp
41:     // ================= TOP BAR (SCAN CONTROLS) =================
42:     // A clean, structured 2-row layout to prevent squishing on smaller windows
43:     QVBoxLayout* controlLayout = new QVBoxLayout();
44:     controlLayout->setSpacing(8);
```
* **Explanation**: Creates a layout manager for scan controls with standard spacing.

```cpp
46:     // Row 1: Grid for Drive and Directory inputs
47:     QGridLayout* inputGrid = new QGridLayout();
48:     inputGrid->setHorizontalSpacing(8);
49:     inputGrid->setVerticalSpacing(6);
```
* **Explanation**: Establishes a responsive grid structure for text fields and their buttons.

```cpp
51:     QLabel* driveLabel = new QLabel(tr("Drive:"), this);
52:     m_driveComboBox = new QComboBox(this);
53:     m_refreshDrivesBtn = new QPushButton(tr("Refresh"), this);
54:     m_refreshDrivesBtn->setToolTip(tr("Refresh available drives"));
```
* **Explanation**: Instantiates drive-selection controls.

```cpp
56:     QLabel* pathLabel = new QLabel(tr("Directory:"), this);
57:     m_pathEdit = new QLineEdit(this);
58:     m_pathEdit->setText("/"); // Default path set to root directory for one-click full scan
59: 
60:     m_browseBtn = new QPushButton(tr("Browse..."), this);
```
* **Explanation**: Creates directory edit boxes and browse trigger buttons.

```cpp
62:     inputGrid->addWidget(driveLabel, 0, 0);
63:     inputGrid->addWidget(m_driveComboBox, 0, 1);
64:     inputGrid->addWidget(m_refreshDrivesBtn, 0, 2);
65: 
66:     inputGrid->addWidget(pathLabel, 1, 0);
67:     inputGrid->addWidget(m_pathEdit, 1, 1);
68:     inputGrid->addWidget(m_browseBtn, 1, 2);
69: 
70:     // Allow combo box and text field to stretch to take available space
71:     inputGrid->setColumnStretch(1, 1);
```
* **Explanation**: Maps controls into grid cells. Column 1 (combobox & line-edit) is assigned a stretch factor of `1` so it expands horizontally to consume window size changes.

```cpp
73:     controlLayout->addLayout(inputGrid);
```
* **Explanation**: Adds the input grid to the top control layout.

```cpp
75:     // Row 2: Horizontal button bar for scan actions
76:     QHBoxLayout* buttonBar = new QHBoxLayout();
77:     buttonBar->setSpacing(8);
```
* **Explanation**: Instantiates a horizontal bar for buttons.

```cpp
79:     m_scanBtn = new QPushButton(tr("Scan"), this);
80:     m_pauseBtn = new QPushButton(tr("Pause"), this);
81:     m_cancelBtn = new QPushButton(tr("Cancel"), this);
82:     m_deleteToggleBtn = new QPushButton(tr("Enable Delete"), this);
83:     m_deleteToggleBtn->setCheckable(true);
```
* **Explanation**: Configures scan and toggle buttons.

```cpp
85:     buttonBar->addWidget(m_scanBtn);
86:     buttonBar->addWidget(m_pauseBtn);
87:     buttonBar->addWidget(m_cancelBtn);
88:     buttonBar->addStretch(1); // Push the deletion toggle to the right
89:     buttonBar->addWidget(m_deleteToggleBtn);
90: 
91:     controlLayout->addLayout(buttonBar);
92: 
93:     mainLayout->addLayout(controlLayout);
```
* **Explanation**: Mounts buttons to the action row, adding a layout spacer (`addStretch`) to push the safety deletion switch to the right.

```cpp
95:     // ================= PROGRESS BAR (ACCURATE LOADING) =================
96:     m_progressBar = new QProgressBar(this);
...
106:     mainLayout->addWidget(m_treeView, 1);
```
* **Explanation**: Inserts loading bar and tree layout configurations.

```cpp
117:     // ================= BOTTOM BAR (HUD STATS) =================
118:     QFrame* hudFrame = new QFrame(this);
...
137:     mainLayout->addWidget(hudFrame);
```
* **Explanation**: Sets up bottom telemetry indicators.

```cpp
139:     // Connect control events
140:     connect(m_driveComboBox, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::onDriveSelected);
141:     connect(m_refreshDrivesBtn, &QPushButton::clicked, this, &MainWindow::refreshDrivesList);
142:     connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
143:     connect(m_scanBtn, &QPushButton::clicked, this, &MainWindow::onScanClicked);
144:     connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
145:     connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelClicked);
146:     connect(m_deleteToggleBtn, &QPushButton::toggled, this, &MainWindow::onDeleteToggleToggled);
```
* **Explanation**: Binds buttons, sliders, and selection boxes to their slots.

```cpp
171:     // Connect scanner finish
172:     connect(m_scanner, &Scanner::finished, this, &MainWindow::onScanFinished);
173: 
174:     // Initial drive listing
175:     refreshDrivesList();
176: }
```
* **Explanation**: Maps scan completion slot and triggers the startup partition discovery.

```cpp
178: void MainWindow::applyDarkTheme() {
...
284: }
```
* **Explanation**: Configures QSS stylesheet. Adds styling overrides for the drive `QComboBox` and its popup item views to conform to the premium dark aesthetic.

```cpp
286: void MainWindow::updateControls() {
287:     bool run = m_scanner->running();
288:     bool pause = m_scanner->paused();
289: 
290:     m_driveComboBox->setEnabled(!run);
291:     m_refreshDrivesBtn->setEnabled(!run);
292:     m_pathEdit->setEnabled(!run);
...
308: }
```
* **Explanation**: Safely toggles state controls. Disables inputs, combos, and refresh options while a background scan is active to prevent resource collisions.

```cpp
310: void MainWindow::updateStatsHUD(uint64_t files, uint64_t dirs, uint64_t bytes) {
311:     m_filesLabel->setText(QString("Files: %1").arg(files));
312:     m_dirsLabel->setText(QString("Folders: %1").arg(dirs));
313:     m_bytesLabel->setText(QString("Size: %1").arg(FileTreeModel::formatSize(bytes)));
314:     m_freeLabel->setText(QString("Free Estate: %1").arg(FileTreeModel::formatSize(m_scanner->free_bytes())));
315: }
```
* **Explanation**: Updates HUD text metrics dynamically.

```cpp
317: void MainWindow::onBrowseClicked() {
318:     QString path = QFileDialog::getExistingDirectory(this, tr("Select Directory to Scan"), m_pathEdit->text());
319:     if (!path.isEmpty()) {
320:         m_pathEdit->setText(path);
321:     }
322: }
```
* **Explanation**: Displays a native directory dialog and updates the path input box text upon selection.

```cpp
324: void MainWindow::onScanClicked() {
325:     QString path = m_pathEdit->text();
326:     if (path.isEmpty()) return;
327: 
328:     m_treeView->setSortingEnabled(false);
329: 
330:     m_statusLabel->setText(tr("Status: Scanning..."));
331:     m_progressBar->setValue(0);
332:     m_progressBar->setFormat(tr("Scanning... %p%"));
333:     m_scanner->start(path.toStdString());
334:     
335:     m_treeModel->setRootNode(nullptr);
336: 
337:     m_guiTimer->start(200); // Refresh tree grid every 200ms
338:     updateControls();
339: }
```
* **Explanation**: Resets progress tracking, sets the status label, and launches the scanner.

```cpp
341: void MainWindow::onPauseClicked() {
342:     if (m_scanner->paused()) {
343:         m_statusLabel->setText(tr("Status: Scanning..."));
344:         m_scanner->resume();
345:     } else {
346:         m_statusLabel->setText(tr("Status: Paused"));
347:         m_scanner->pause();
348:     }
349:     updateControls();
350: }
```
* **Explanation**: Toggles scanning pause/resume.

```cpp
352: void MainWindow::onCancelClicked() {
353:     m_statusLabel->setText(tr("Status: Cancelling..."));
354:     m_scanner->cancel();
355:     updateControls();
356: }
```
* **Explanation**: Cancels the current scan.

```cpp
358: void MainWindow::onScanFinished() {
359:     m_guiTimer->stop();
...
390: }
```
* **Explanation**: Handles layout, column resizing, and updating telemetry indicators upon scan completion.

```cpp
392: void MainWindow::onUpdateTimer() {
393:     // Inform model that data changed to update QTreeView in real-time
394:     m_treeModel->layoutChanged();
395: 
396:     // Update neon sky blue progress bar value
397:     m_progressBar->setValue(static_cast<int>(m_scanner->progress_percentage()));
398: 
399:     updateStatsHUD(m_scanner->files_scanned(), m_scanner->dirs_scanned(), m_scanner->bytes_scanned());
400: }
```
* **Explanation**: Periodically updates tree layouts and HUD metrics.

```cpp
402: void MainWindow::onDeleteToggleToggled(bool checked) {
...
426: }
```
* **Explanation**: Toggles the custom stylesheet overrides and enables/disables the delete keys.

```cpp
428: void MainWindow::onCustomContextMenuRequested(const QPoint& pos) {
...
454: }
```
* **Explanation**: Houses context menu definitions on right-click.

```cpp
456: QString MainWindow::getAbsoluteNodePath(TreeNode* node) {
...
474: }
```
* **Explanation**: Reconstructs absolute directory paths from tree nodes.

```cpp
476: void MainWindow::propagate_deletion_stats(...) {
...
493: }
```
* **Explanation**: Feeds deletion counts to parent folders with safe underflow clamping.

```cpp
495: void MainWindow::onDeleteActionTriggered() {
...
565: }
```
* **Explanation**: Checks for symlinks/TOCTOU security hazards, prompts confirmation, and moves selected items to the trash folder.

```cpp
567: void MainWindow::refreshDrivesList() {
568:     m_driveComboBox->blockSignals(true);
569:     m_driveComboBox->clear();
570: 
571:     bool parsedWithLsblk = false;
```
* **Explanation**: Begins drive discovery. Blocks widget events to prevent selection change feedback loops, and clears old items.

```cpp
573: #ifdef __linux__
574:     // Try running lsblk to fetch both mounted and unmounted block devices
575:     QProcess process;
576:     process.start("lsblk", QStringList() << "-p" << "-o" << "NAME,FSTYPE,SIZE,MOUNTPOINT,LABEL,UUID" << "-J");
```
* **Explanation**: Spawns system process `lsblk` on Linux requesting paths, filesystem formats, partition sizes, labels, and mount points in structured JSON format.

```cpp
577:     if (process.waitForFinished(3000)) {
578:         QByteArray output = process.readAllStandardOutput();
579:         QJsonDocument doc = QJsonDocument::fromJson(output);
580:         if (!doc.isNull() && doc.isObject()) {
581:             QJsonObject rootObj = doc.object();
582:             if (rootObj.contains("blockdevices")) {
583:                 QJsonArray devices = rootObj["blockdevices"].toArray();
584:                 parseBlockDevices(devices);
585:                 parsedWithLsblk = true;
586:             }
587:         }
588:     }
589: #endif
```
* **Explanation**: Waits up to 3 seconds for compilation output, reads standard output, parses the JSON payload, and routes child arrays to block parser slots.

```cpp
591:     // Fall back to QStorageInfo if lsblk wasn't used or failed
592:     if (!parsedWithLsblk) {
...
645:     }
```
* **Explanation**: Fallback loader. If not running on Linux or if `lsblk` fails, falls back to querying mounted volumes using `QStorageInfo`.

```cpp
647:     // Set default selection (System Root /) if available
648:     int rootIndex = -1;
...
671: }
```
* **Explanation**: Finds system root `/` in the populated list. Sets the combo selection to it and loads `/` into the path text edit. Unblocks signals.

```cpp
673: void MainWindow::parseBlockDevices(const QJsonArray& devices) {
674:     for (const QJsonValue& devVal : devices) {
675:         QJsonObject devObj = devVal.toObject();
676:         if (devObj.contains("children")) {
677:             QJsonArray children = devObj["children"].toArray();
678:             parseBlockDevices(children); // recursively parse partitions/children
679:         } else {
680:             processPartition(devObj);
681:         }
682:     }
683: }
```
* **Explanation**: Recurses down physical disks to identify individual partition children.

```cpp
685: void MainWindow::processPartition(const QJsonObject& partObj) {
...
693:     if (fstype.isEmpty()) return;
```
* **Explanation**: Skips swap, unformatted partition tables, and raw sectors.

```cpp
695:     // Filter out typical virtual/special loop filesystems
696:     if (fstype == "tmpfs" || fstype == "devtmpfs" || fstype == "sysfs" || 
697:         fstype == "proc" || fstype == "squashfs" || fstype == "cgroup") {
...
705:     }
```
* **Explanation**: Ignores system loops, cgroups, virtual nodes, and tmp folders.

```cpp
707:     if (!mountpoint.isEmpty()) {
...
712:         m_driveComboBox->addItem(text, mountpoint);
```
* **Explanation**: Adds mounted partitions to the selection list, storing their mount path as item data.

```cpp
713:     } else {
...
720:         m_driveComboBox->addItem(text, "device:" + name);
721:     }
722: }
```
* **Explanation**: Adds unmounted partitions to the list, labeling them `[Unmounted - Click to Mount]` and storing their raw device node (`device:/dev/sdaX`) as selection data.

```cpp
724: void MainWindow::onDriveSelected(int index) {
...
729:     if (pathData.startsWith("device:")) {
730:         QString devicePath = pathData.mid(7); // Extract raw path like "/dev/sda1"
```
* **Explanation**: User selected a drive. Checks if it is unmounted (starts with `device:`).

```cpp
732:         m_statusLabel->setText(tr("Status: Mounting %1...").arg(devicePath));
733:         qApp->processEvents();
```
* **Explanation**: Sets UI labels and forces event loops to repaint immediately.

```cpp
735:         // 1. Try mounting normally via udisksctl
736:         QProcess mountProc;
737:         mountProc.start("udisksctl", QStringList() << "mount" << "-b" << devicePath);
```
* **Explanation**: Spawns `udisksctl` to mount the block device node, which triggers standard polkit elevation handles on modern desktops without asking for sudo root.

```cpp
738:         if (!mountProc.waitForFinished(8000)) {
...
742:         }
```
* **Explanation**: Sets an 8-second time boundary. Aborts with timeout warnings on failure.

```cpp
744:         QString output = QString::fromUtf8(mountProc.readAllStandardOutput()).trimmed();
...
746:         bool success = (mountProc.exitCode() == 0);
```
* **Explanation**: Reads shell return outputs to check for success.

```cpp
748:         // 2. If it fails, try read-only fallback mount (extremely useful for dirty NTFS logs)
749:         if (!success) {
750:             mountProc.start("udisksctl", QStringList() << "mount" << "-b" << devicePath << "-o" << "ro");
...
756:         }
```
* **Explanation**: Fallback mount. If the drive has log inconsistencies (like NTFS fast-boot blocks), attempts read-only mounting `-o ro` which bypasses write locks.

```cpp
758:         if (success) {
759:             // Find the mount point from the output, e.g. "Mounted /dev/sda1 at /run/media/ethan/54BE3B12BE3AEC5C"
760:             int atIndex = output.indexOf(" at ");
761:             if (atIndex != -1) {
762:                 QString mountPath = output.mid(atIndex + 4).trimmed();
763:                 m_pathEdit->setText(mountPath);
764:                 
765:                 // Refresh list so the drive now appears as mounted
766:                 refreshDrivesList();
```
* **Explanation**: On success, extracts mount paths from outputs, feeds it to the Directory field, and refreshes volume listings.

```cpp
768:                 // Select the newly mounted path in the combobox
769:                 for (int i = 0; i < m_driveComboBox->count(); ++i) {
...
774:                 }
```
* **Explanation**: Finds and selects the newly-mounted volume in the selection dropdown.

```cpp
782:         // Display detailed error dialog if mount failed completely
783:         QMessageBox::critical(this, tr("Mount Failed"), 
...
787:         refreshDrivesList();
```
* **Explanation**: Mount failed. Displays dialog windows indicating shell errors, and updates device lists.

```cpp
788:     } else {
789:         m_pathEdit->setText(pathData);
790:     }
791: }
```
* **Explanation**: Mounted selection case. Writes selection paths straight to directory fields.
