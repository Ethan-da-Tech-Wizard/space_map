# SpaceMap Architecture Book: Chapter 7

This chapter provides a complete line-by-line, character-by-character architectural explanation of the Graphical User Interface Window (`src/main_window.hpp` and `src/main_window.cpp`).

---

# CHAPTER 7: Graphical User Interface & File Recycling Engine

The user interface of SpaceMap is written in native C++ using **Qt6 widgets**. It coordinates the scanning engine, updates data dashboards, handles user interactions, and provides a safe file deletion tool. It is styled with custom Qt Style Sheets (QSS) for a modern dark-mode aesthetic with smooth interactions.

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
11: #include "scanner.hpp"
12: #include "file_tree_model.hpp"
```
* **Explanation**: Imports Qt layout elements:
  * `<QMainWindow>`: Provides a main application window structure (with menu bars, status bars, and central widgets).
  * `<QLineEdit>`: Single-line input box.
  * `<QPushButton>`: Standard click button.
  * `<QLabel>`: Static text headers.
  * `<QTreeView>`: Grid layout.
  * `<QProgressBar>`: Load progress bars.
  * `"scanner.hpp"` and `"file_tree_model.hpp"`: Connects GUI widgets to the scanning engine and data model.

```cpp
14: class MainWindow : public QMainWindow {
15:     Q_OBJECT
```
* **Explanation**:
  * Inherits from `QMainWindow`.
  * `Q_OBJECT`: Mandatory macro enabling signals, slots, and metadata.

```cpp
16: public:
17:     explicit MainWindow(QWidget* parent = nullptr);
18:     ~MainWindow() override = default;
```
* **Explanation**: Constructor and virtual destructor.

```cpp
20: private slots:
21:     void onBrowseClicked();
22:     void onScanClicked();
23:     void onPauseClicked();
24:     void onCancelClicked();
25:     void onScanFinished();
26:     void onUpdateTimer();
27:     void onDeleteToggleToggled(bool checked);
28:     void onDeleteActionTriggered();
29:     void onCustomContextMenuRequested(const QPoint& pos);
```
* **Explanation**: Private slots connected to user interaction events (clicks, selections, timers).

```cpp
31: private:
32:     void setupUi();
33:     void applyDarkTheme();
34:     void updateControls();
35:     void updateStatsHUD(uint64_t files, uint64_t dirs, uint64_t bytes);
36:     QString getAbsoluteNodePath(TreeNode* node);
37:     void propagate_deletion_stats(TreeNode* start_node, uint64_t size_removed, uint64_t allocated_removed, uint64_t files_removed, uint64_t dirs_removed);
```
* **Explanation**:
  * Private helper functions for UI initialization, styles, control states, absolute path resolution, and deletion stats propagation.

```cpp
40:     Scanner* m_scanner = nullptr;
41:     FileTreeModel* m_treeModel = nullptr;
```
* **Explanation**: Pointers referencing the concurrent scanner and tree model.

```cpp
44:     QLineEdit* m_pathEdit = nullptr;
45:     QPushButton* m_browseBtn = nullptr;
46:     QPushButton* m_scanBtn = nullptr;
47:     QPushButton* m_pauseBtn = nullptr;
48:     QPushButton* m_cancelBtn = nullptr;
49:     QPushButton* m_deleteToggleBtn = nullptr;
50:     QAction* m_deleteAction = nullptr;
```
* **Explanation**: Control widget pointers.

```cpp
54:     QLabel* m_statusLabel = nullptr;
55:     QLabel* m_filesLabel = nullptr;
56:     QLabel* m_dirsLabel = nullptr;
57:     QLabel* m_bytesLabel = nullptr;
58:     QLabel* m_freeLabel = nullptr;
```
* **Explanation**: Pointers to the text labels on the bottom HUD panel.

```cpp
62:     QTreeView* m_treeView = nullptr;
63:     QProgressBar* m_progressBar = nullptr;
64:     QTimer* m_guiTimer = nullptr;
65: };
```
* **Explanation**: Pointers to the tree view, progress bar, and timer.

---

## 2. src/main_window.cpp (Source File Breakdown)

The source file handles layout construction, stylesheets, events mapping, path walking, and file trashing operations.

```cpp
17: MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
18:     m_scanner = new Scanner(this);
19:     m_treeModel = new FileTreeModel(nullptr, this);
```
* **Explanation**: Constructor. Initializes the parent-child relationships for automatic memory cleanup.

```cpp
21:     m_guiTimer = new QTimer(this);
22:     connect(m_guiTimer, &QTimer::timeout, this, &MainWindow::onUpdateTimer);
```
* **Explanation**: Instantiates the GUI refresh timer and connects its `timeout` signal to `onUpdateTimer`.

```cpp
24:     setupUi();
25:     applyDarkTheme();
26:     updateControls();
27: }
```
* **Explanation**: Builds layout geometry, applies styles, and initializes control button states.

```cpp
29: void MainWindow::setupUi() {
30:     setWindowTitle("SpaceMap - C++ High-Speed Disk Analyzer");
```
* **Explanation**: Sets window title bar text.

```cpp
32:     QWidget* centralWidget = new QWidget(this);
33:     setCentralWidget(centralWidget);
```
* **Explanation**: Creates a container widget to act as the main window's layout area.

```cpp
35:     QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
36:     mainLayout->setContentsMargins(10, 10, 10, 10);
37:     mainLayout->setSpacing(10);
```
* **Explanation**:
  * `QVBoxLayout`: Arranges child widgets vertically.
  * Adds 10-pixel padding margins and 10-pixel vertical gaps between widgets.

```cpp
40:     QHBoxLayout* topBar = new QHBoxLayout();
41:     topBar->setSpacing(8);
```
* **Explanation**:
  * `QHBoxLayout`: Arranges controls horizontally.
  * Sets the gap between controls to 8 pixels.

```cpp
43:     QLabel* pathLabel = new QLabel(tr("Directory:"), this);
44:     m_pathEdit = new QLineEdit(this);
45:     m_pathEdit->setText("/"); // Default path set to root directory for one-click full scan
```
* **Explanation**: Creates the path input box, defaulting to the root directory `/`.

```cpp
47:     m_browseBtn = new QPushButton(tr("Browse..."), this);
48:     m_scanBtn = new QPushButton(tr("Scan"), this);
49:     m_pauseBtn = new QPushButton(tr("Pause"), this);
50:     m_cancelBtn = new QPushButton(tr("Cancel"), this);
51:     m_deleteToggleBtn = new QPushButton(tr("Enable Delete"), this);
52:     m_deleteToggleBtn->setCheckable(true);
```
* **Explanation**:
  * Instantiates control buttons.
  * `setCheckable(true)`: Makes the delete button act as a toggle switch.

```cpp
54:     topBar->addWidget(pathLabel);
55:     topBar->addWidget(m_pathEdit, 1);
```
* **Explanation**: Adds directory input controls to the horizontal top bar. `m_pathEdit, 1` configures the input box with a stretch factor of 1, allowing it to expand horizontally to fill available space.

```cpp
56:     topBar->addWidget(m_browseBtn);
57:     topBar->addWidget(m_scanBtn);
58:     topBar->addWidget(m_pauseBtn);
59:     topBar->addWidget(m_cancelBtn);
60:     topBar->addWidget(m_deleteToggleBtn);
61: 
62:     mainLayout->addLayout(topBar);
```
* **Explanation**: Appends buttons to the top bar layout and adds the top bar to the vertical main layout.

```cpp
65:     m_progressBar = new QProgressBar(this);
66:     m_progressBar->setValue(0);
67:     m_progressBar->setTextVisible(true);
68:     m_progressBar->setFormat(tr("Idle - 0%"));
69:     mainLayout->addWidget(m_progressBar);
```
* **Explanation**: Constructs the progress bar and sets its initial values.

```cpp
72:     m_treeView = new QTreeView(this);
73:     m_treeView->setModel(m_treeModel);
74:     m_treeView->setItemDelegateForColumn(1, new PercentDelegate(this));
```
* **Explanation**:
  * Instantiates the Tree View.
  * Binds the tree model.
  * Registers `PercentDelegate` for Column 1 (`% of Parent`) to draw cell ratio bars.

```cpp
75:     m_treeView->setAlternatingRowColors(true);
76:     m_treeView->setSortingEnabled(false); // Sorted by size internally when scan completes
77:     m_treeView->setIndentation(12); // Compact indentation to prevent clipping in deep paths
78:     m_treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
79:     m_treeView->header()->setSectionResizeMode(QHeaderView::Interactive);
80:     m_treeView->header()->setDefaultSectionSize(180);
81:     m_treeView->header()->setStretchLastSection(true);
82:     
83:     mainLayout->addWidget(m_treeView, 1); // Tree view occupies all available vertical space
```
* **Explanation**:
  * `setIndentation(12)`: Sets the indentation offset of subfolders to 12 pixels, preventing deeply nested directories from sliding off the screen horizontally.
  * `Interactive`: Allows the user to manually resize column widths.
  * `setStretchLastSection(true)`: Forces the last column to expand and fill the remaining horizontal space.
  * `mainLayout->addWidget(m_treeView, 1)`: Adds the tree view to the main layout with a stretch factor of 1 to occupy all available vertical space.

```cpp
86:     QFrame* hudFrame = new QFrame(this);
87:     hudFrame->setObjectName("hudFrame");
88:     QHBoxLayout* hudLayout = new QHBoxLayout(hudFrame);
89:     hudLayout->setContentsMargins(15, 8, 15, 8);
90:     hudLayout->setSpacing(25);
```
* **Explanation**: Constructs the bottom HUD panel using `QFrame` and assigns it the ID `hudFrame` for targeted QSS styling.

```cpp
92:     m_statusLabel = new QLabel(tr("Status: Idle"), this);
93:     m_filesLabel = new QLabel(tr("Files: 0"), this);
94:     m_dirsLabel = new QLabel(tr("Folders: 0"), this);
95:     m_bytesLabel = new QLabel(tr("Size: 0 B"), this);
96:     m_freeLabel = new QLabel(tr("Free Estate: 0 B"), this);
```
* **Explanation**: Instantiates HUD labels.

```cpp
98:     hudLayout->addWidget(m_statusLabel);
99:     hudLayout->addWidget(m_filesLabel);
100:     hudLayout->addWidget(m_dirsLabel);
101:     hudLayout->addWidget(m_bytesLabel);
102:     hudLayout->addWidget(m_freeLabel);
103:     hudLayout->addStretch(1);
104: 
105:     mainLayout->addWidget(hudFrame);
```
* **Explanation**: Adds HUD widgets and layouts to the main window.

```cpp
108:     connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
109:     connect(m_scanBtn, &QPushButton::clicked, this, &MainWindow::onScanClicked);
110:     connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
111:     connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelClicked);
112:     connect(m_deleteToggleBtn, &QPushButton::toggled, this, &MainWindow::onDeleteToggleToggled);
```
* **Explanation**: Binds control signals to slots.

```cpp
115:     m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
116:     connect(m_treeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onCustomContextMenuRequested);
```
* **Explanation**: Enables custom context menus on the tree view.

```cpp
118:     m_deleteAction = new QAction(tr("Delete to Trash"), this);
119:     m_deleteAction->setShortcut(QKeySequence::Delete);
120:     m_deleteAction->setShortcutContext(Qt::WidgetShortcut);
121:     m_deleteAction->setEnabled(false); // Disabled by default
122:     connect(m_deleteAction, &QAction::triggered, this, &MainWindow::onDeleteActionTriggered);
123:     m_treeView->addAction(m_deleteAction);
```
* **Explanation**: Creates a deletion action, maps it to the `Delete` key, and sets its context to `WidgetShortcut` so it only triggers when the tree view has focus.

```cpp
126:     connect(m_treeView, &QTreeView::expanded, this, [this](const QModelIndex&) {
127:         QTimer::singleShot(0, this, [this]() {
128:             m_treeView->resizeColumnToContents(0);
129:         });
130:     });
131:     connect(m_treeView, &QTreeView::collapsed, this, [this](const QModelIndex&) {
132:         QTimer::singleShot(0, this, [this]() {
133:             m_treeView->resizeColumnToContents(0);
134:         });
135:     });
```
* **Explanation**:
  * **Dynamic Column Auto-Resize**: Expanded/collapsed signals trigger a single-shot timer with `0` milliseconds. This defers the column resizing until the layout engine has updated the display, preventing text clipping.

```cpp
138:     connect(m_scanner, &Scanner::finished, this, &MainWindow::onScanFinished);
139: }
```
* **Explanation**: Connects scanner finished signals to the window's slot.

```cpp
141: void MainWindow::applyDarkTheme() {
```
* **Explanation**: Applies stylesheet styling.

```cpp
143:     QString stylesheet = R"(
144:         QMainWindow {
145:             background-color: #121212;
146:         }
```
* **Explanation**: Styles window backgrounds to a dark grey `#121212`.

```cpp
151:         QProgressBar {
152:             background-color: #1a1a1a;
153:             color: #ffffff;
154:             border: 1px solid #2d2d2d;
155:             border-radius: 4px;
156:             text-align: center;
157:             height: 18px;
158:             font-size: 11px;
159:             font-weight: bold;
160:         }
161:         QProgressBar::chunk {
162:             background-color: #00d2ff;
163:             border-radius: 3px;
164:         }
```
* **Explanation**: Styles the progress bar with a dark background and a neon sky-blue loading indicator.

```cpp
165:         QLineEdit {
166:             background-color: #1e1e1e;
167:             color: #ffffff;
168:             border: 1px solid #333333;
169:             padding: 6px;
170:             border-radius: 4px;
171:             font-size: 13px;
172:         }
173:         QLineEdit:focus {
174:             border: 1px solid #007acc;
175:         }
```
* **Explanation**: Styles the input box. Highlights the borders in blue (`#007acc`) when focused.

```cpp
176:         QPushButton {
177:             background-color: #2d2d2d;
178:             color: #e0e0e0;
179:             border: 1px solid #3a3a3a;
180:             padding: 6px 12px;
181:             border-radius: 4px;
182:             font-size: 13px;
183:         }
184:         QPushButton:hover {
185:             background-color: #3d3d3d;
186:             border: 1px solid #505050;
187:         }
188:         QPushButton:pressed {
189:             background-color: #005999;
190:         }
```
* **Explanation**: Styles standard buttons with hover and pressed states.

```cpp
191:         QTreeView {
192:             background-color: #1a1a1a;
193:             color: #e0e0e0;
194:             border: 1px solid #2d2d2d;
195:             border-radius: 4px;
196:             alternate-background-color: #222222;
197:         }
```
* **Explanation**: Styles the tree view and alternates row colors (`#222222` vs `#1a1a1a`) to improve readability.

```cpp
217:     setStyleSheet(stylesheet);
218: }
```
* **Explanation**: Applies the stylesheet to the window.

```cpp
220: void MainWindow::updateControls() {
221:     bool run = m_scanner->running();
222:     bool pause = m_scanner->paused();
```
* **Explanation**: Evaluates running states.

```cpp
224:     m_pathEdit->setEnabled(!run);
225:     m_browseBtn->setEnabled(!run);
226:     m_scanBtn->setEnabled(!run);
227:     
228:     m_pauseBtn->setEnabled(run);
229:     m_pauseBtn->setText(pause ? tr("Resume") : tr("Pause"));
230:     
231:     m_cancelBtn->setEnabled(run);
```
* **Explanation**: Enables or disables input fields and scan control buttons depending on the active scan state.

```cpp
233:     if (run) {
234:         m_deleteToggleBtn->setEnabled(false);
235:         m_deleteAction->setEnabled(false);
236:     } else {
237:         m_deleteToggleBtn->setEnabled(true);
238:         m_deleteAction->setEnabled(m_deleteToggleBtn->isChecked());
239:     }
240: }
```
* **Explanation**:
  * **Scan Deletion Guard**: Disables the deletion toggle button and action while a scan is running, preventing modification conflicts.

```cpp
242: void MainWindow::updateStatsHUD(uint64_t files, uint64_t dirs, uint64_t bytes) {
243:     m_filesLabel->setText(QString("Files: %1").arg(files));
244:     m_dirsLabel->setText(QString("Folders: %1").arg(dirs));
245:     m_bytesLabel->setText(QString("Size: %1").arg(FileTreeModel::formatSize(bytes)));
246:     m_freeLabel->setText(QString("Free Estate: %1").arg(FileTreeModel::formatSize(m_scanner->free_bytes())));
247: }
```
* **Explanation**: Updates HUD text metrics dynamically.

```cpp
249: void MainWindow::onBrowseClicked() {
250:     QString path = QFileDialog::getExistingDirectory(this, tr("Select Directory to Scan"), m_pathEdit->text());
251:     if (!path.isEmpty()) {
252:         m_pathEdit->setText(path);
253:     }
254: }
```
* **Explanation**: Displays a native directory dialog and updates the path input box text upon selection.

```cpp
256: void MainWindow::onScanClicked() {
257:     QString path = m_pathEdit->text();
258:     if (path.isEmpty()) return;
259: 
260:     m_treeView->setSortingEnabled(false);
```
* **Explanation**:
  * Starts scanning.
  * `setSortingEnabled(false)`: Disables sorting while scanning to prevent performance degradation from updating layouts dynamically.

```cpp
262:     m_statusLabel->setText(tr("Status: Scanning..."));
263:     m_progressBar->setValue(0);
264:     m_progressBar->setFormat(tr("Scanning... %p%"));
265:     m_scanner->start(path.toStdString());
```
* **Explanation**: Resets progress tracking, sets the status label, and launches the scanner.

```cpp
267:     m_treeModel->setRootNode(nullptr);
268: 
269:     m_guiTimer->start(200); // Refresh tree grid every 200ms
270:     updateControls();
271: }
```
* **Explanation**: Clears the previous tree, starts the GUI update timer (firing every 200ms), and updates control button states.

```cpp
273: void MainWindow::onPauseClicked() {
274:     if (m_scanner->paused()) {
275:         m_statusLabel->setText(tr("Status: Scanning..."));
276:         m_scanner->resume();
277:     } else {
278:         m_statusLabel->setText(tr("Status: Paused"));
279:         m_scanner->pause();
280:     }
281:     updateControls();
282: }
```
* **Explanation**: Toggles scanning pause/resume.

```cpp
284: void MainWindow::onCancelClicked() {
285:     m_statusLabel->setText(tr("Status: Cancelling..."));
286:     m_scanner->cancel();
287:     updateControls();
288: }
```
* **Explanation**: Cancels the current scan.

```cpp
290: void MainWindow::onScanFinished() {
291:     m_guiTimer->stop();
```
* **Explanation**: Scan completed. Stops the update timer.

```cpp
293:     TreeNode* root = m_scanner->root_node();
294:     m_treeModel->setRootNode(root);
```
* **Explanation**: Binds the root node of the scanned filesystem tree to the model.

```cpp
296:     // Set sort indicator first to prevent redundant sorting when enabling sorting
297:     m_treeView->header()->setSortIndicator(2, Qt::DescendingOrder);
298:     m_treeView->setSortingEnabled(true);
```
* **Explanation**: Sets the sort indicator column to size (Column 2) and order to descending, then enables sorting.

```cpp
300:     // Collapse all items to show the clean base-level view
301:     m_treeView->collapseAll();
```
* **Explanation**: Collapses all directory branches to present a clean root directory view.

```cpp
303:     // Dynamically resize all columns to contents initially
304:     for (int i = 0; i < 6; ++i) {
305:         m_treeView->resizeColumnToContents(i);
306:     }
```
* **Explanation**: Resizes all 6 columns to fit their content.

```cpp
308:     // 4. Update HUD and Progress Bar
309:     if (m_scanner->cancelled()) {
310:         m_statusLabel->setText(tr("Status: Cancelled"));
311:         m_progressBar->setFormat(tr("Cancelled - %p%"));
312:     } else if (m_scanner->files_scanned() == 0 && m_scanner->dirs_scanned() == 0) {
313:         m_statusLabel->setText(tr("Status: Failed/Empty"));
314:         m_progressBar->setFormat(tr("Failed/Empty"));
315:     } else {
316:         m_statusLabel->setText(tr("Status: Complete"));
317:         m_progressBar->setValue(100);
318:         m_progressBar->setFormat(tr("Complete - 100%"));
319:     }
320:     updateStatsHUD(m_scanner->files_scanned(), m_scanner->dirs_scanned(), m_scanner->bytes_scanned());
321:     updateControls();
322: }
```
* **Explanation**: Updates HUD text and progress displays based on scan results.

```cpp
324: void MainWindow::onUpdateTimer() {
325:     // Inform model that data changed to update QTreeView in real-time
326:     m_treeModel->layoutChanged();
```
* **Explanation**: Notifies the tree view that the model layout has changed, prompting it to refresh visible items.

```cpp
328:     // Update neon sky blue progress bar value
329:     m_progressBar->setValue(static_cast<int>(m_scanner->progress_percentage()));
330: 
331:     updateStatsHUD(m_scanner->files_scanned(), m_scanner->dirs_scanned(), m_scanner->bytes_scanned());
332: }
```
* **Explanation**: Updates progress and HUD statistics.

```cpp
334: void MainWindow::onDeleteToggleToggled(bool checked) {
335:     if (checked) {
336:         QMessageBox::StandardButton reply = QMessageBox::warning(
337:             this,
338:             tr("Enable File Deletion"),
339:             tr("<font color='black'>Are you sure you wish to enable file deletion?<br>This allows you to permanently move files and folders to the recycle bin. Please use with caution.</font>"),
340:             QMessageBox::Yes | QMessageBox::No,
341:             QMessageBox::No
342:         );
```
* **Explanation**:
  * **OS Contrast Bypass**: Standard Qt warning boxes can suffer from text contrast issues on systems with dark themes enabled, resulting in white text on white backgrounds. Wrapping the message text in `<font color='black'>` tags forces the message box to render text in black, ensuring it is readable on all platforms.

```cpp
343:         if (reply == QMessageBox::Yes) {
344:             m_deleteAction->setEnabled(true);
345:             m_deleteToggleBtn->setStyleSheet("background-color: #ff3333; color: white; border: 1px solid #ff6666; font-weight: bold;");
346:             m_deleteToggleBtn->setText(tr("Deletion Enabled"));
347:         } else {
348:             m_deleteToggleBtn->blockSignals(true);
349:             m_deleteToggleBtn->setChecked(false);
350:             m_deleteToggleBtn->blockSignals(false);
351:             m_deleteAction->setEnabled(false);
352:         }
```
* **Explanation**:
  * If the user selects **Yes**, enables the deletion action and styles the toggle button in bright red (`#ff3333`) to indicate deletion capability is enabled.
  * If **No**, blocks signals temporarily, resets the toggle button state, and disables deletion actions.

```cpp
353:     } else {
354:         m_deleteAction->setEnabled(false);
355:         m_deleteToggleBtn->setStyleSheet("");
356:         m_deleteToggleBtn->setText(tr("Enable Delete"));
357:     }
358: }
```
* **Explanation**: Disables deletion capability and resets button styling.

```cpp
360: void MainWindow::onCustomContextMenuRequested(const QPoint& pos) {
361:     QModelIndex index = m_treeView->indexAt(pos);
362:     if (!index.isValid()) return;
```
* **Explanation**: Right-click Context Menu. If the user right-clicked empty space in the tree table, returns.

```cpp
364:     QMenu menu(this);
365:     menu.setStyleSheet(R"(
366:         QMenu {
367:             background-color: #1e1e1e;
368:             color: #ffffff;
369:             border: 1px solid #333333;
370:         }
371:         QMenu::item:selected {
372:             background-color: #007acc;
373:             color: #ffffff;
374:         }
375:         QMenu::item:disabled {
376:             color: #666666;
377:         }
378:     )");
```
* **Explanation**: Styles the right-click menu.

```cpp
380:     QAction* deleteAct = new QAction(tr("Delete to Trash"), &menu);
381:     deleteAct->setEnabled(m_deleteToggleBtn->isChecked());
382:     connect(deleteAct, &QAction::triggered, this, &MainWindow::onDeleteActionTriggered);
383:     menu.addAction(deleteAct);
```
* **Explanation**: Adds the "Delete to Trash" option to the menu, enabled only if the deletion toggle button is checked.

```cpp
385:     menu.exec(m_treeView->viewport()->mapToGlobal(pos));
386: }
```
* **Explanation**: Converts coordinates from the tree view's internal coordinates to screen-global coordinates and displays the context menu.

```cpp
388: QString MainWindow::getAbsoluteNodePath(TreeNode* node) {
389:     if (!node) return QString();
390:     std::string path = node->name;
391:     TreeNode* curr = node->parent;
392:     while (curr) {
393:         if (curr->name == "/") {
394:             path = "/" + path;
395:         } else {
396:             path = curr->name + "/" + path;
397:         }
398:         curr = curr->parent;
399:     }
```
* **Explanation**:
  * **Absolute Path Reconstruction**: Reconstructs the absolute filesystem path by walking up the parent pointers starting from the selected node and prepending parent directory names to the path string.

```cpp
401:     // Clean up double slashes
402:     size_t pos;
403:     while ((pos = path.find("//")) != std::string::npos) {
404:         path.replace(pos, 2, "/");
405:     }
406:     return QString::fromStdString(path);
407: }
```
* **Explanation**: Removes duplicate slashes introduced during path construction.

```cpp
409: void MainWindow::propagate_deletion_stats(TreeNode* start_node, uint64_t size_removed, uint64_t allocated_removed, uint64_t files_removed, uint64_t dirs_removed) {
410:     TreeNode* current = start_node;
411:     while (current != nullptr) {
412:         if (current->size >= size_removed) current->size -= size_removed;
413:         else current->size = 0;
```
* **Explanation**:
  * **Safe Stats Propagation**: Subtracts the stats of the deleted file/folder from parent directories.
  * **Underflow Guard**: Since sizes are stored as unsigned 64-bit integers (`uint64_t`), subtracting a value larger than the current size results in an integer underflow, wrapping the value around to a massive number. The check ensures the value is clamped to 0 instead.

```cpp
415:         if (current->allocated_size >= allocated_removed) current->allocated_size -= allocated_removed;
416:         else current->allocated_size = 0;
417: 
418:         if (current->file_count >= files_removed) current->file_count -= files_removed;
419:         else current->file_count = 0;
420: 
421:         if (current->dir_count >= dirs_removed) current->dir_count -= dirs_removed;
422:         else current->dir_count = 0;
423: 
424:         current = current->parent;
425:     }
426: }
```
* **Explanation**: Subtracts statistics with underflow guards and moves to the parent node until reaching the root.

```cpp
428: void MainWindow::onDeleteActionTriggered() {
429:     if (m_scanner->running()) {
430:         QMessageBox::warning(this, tr("Delete Error"), tr("<font color='black'>Cannot delete files while scanning is active. Please wait for the scan to finish or cancel it first.</font>"));
431:         return;
432:     }
```
* **Explanation**:
  * **Active Scan Check**: Aborts deletion if a scan is currently running to prevent concurrency conflicts with background threads.

```cpp
434:     QModelIndex index = m_treeView->currentIndex();
435:     if (!index.isValid()) return;
436: 
437:     TreeNode* node = static_cast<TreeNode*>(index.internalPointer());
438:     if (!node) return;
```
* **Explanation**: Resolves the selected index and retrieves its `TreeNode` pointer.

```cpp
440:     if (!node->parent) {
441:         QMessageBox::warning(this, tr("Delete Error"), tr("<font color='black'>Cannot delete the root scanned folder itself.</font>"));
442:         return;
443:     }
```
* **Explanation**: Blocks deleting the scanned root directory.

```cpp
445:     if (!m_deleteToggleBtn->isChecked()) {
446:         QMessageBox::warning(this, tr("Delete Disabled"), tr("<font color='black'>File deletion is disabled. Please toggle 'Enable Delete' first.</font>"));
447:         return;
448:     }
```
* **Explanation**: Verification check. Aborts if deletion has been disabled.

```cpp
450:     QString absolutePath = getAbsoluteNodePath(node);
451:     if (absolutePath.isEmpty()) return;
```
* **Explanation**: Reconstructs the absolute path of the selected node.

```cpp
453:     QFileInfo fileInfo(absolutePath);
454:     if (!fileInfo.exists()) {
455:         QMessageBox::warning(this, tr("Delete Error"), tr("<font color='black'>File no longer exists.</font>"));
456:         return;
457:     }
458:     if (fileInfo.isSymLink()) {
459:         QMessageBox::warning(this, tr("Delete Error"), tr("<font color='black'>Security Alert: Symbolic link detected. Deletion aborted to prevent safety issues.</font>"));
460:         return;
461:     }
```
* **Explanation**:
  * **Symlink & Existence Check (TOCTOU prevention)**: Verifies the file still exists at the path and guarantees it is **not** a symbolic link. If a malicious process replaces the directory with a symlink right before deletion, the check triggers a warning and aborts the deletion.

```cpp
463:     QMessageBox::StandardButton reply = QMessageBox::question(
464:         this,
465:         tr("Confirm Delete"),
466:         tr("<font color='black'>Are you sure you wish to delete this %1?<br>Path: %2<br><br>It will be moved to the recycle bin/trash.</font>")
467:             .arg(node->is_dir ? tr("folder") : tr("file"))
468:             .arg(absolutePath),
469:         QMessageBox::Yes | QMessageBox::No,
470:         QMessageBox::No
471:     );
```
* **Explanation**: Displays a confirmation dialog before deleting the item.

```cpp
473:     if (reply == QMessageBox::Yes) {
474:         bool success = QFile::moveToTrash(absolutePath);
```
* **Explanation**:
  * **Safe Deletion**:
  * `QFile::moveToTrash(absolutePath)`: Moves the item to the system's Recycle Bin or Trash instead of deleting it permanently, allowing recovery if deleted accidentally.

```cpp
475:         if (success) {
476:             uint64_t size_removed = node->size;
477:             uint64_t allocated_removed = node->allocated_size;
478:             uint64_t files_removed = node->is_dir ? node->file_count.load() : 1;
479:             uint64_t dirs_removed = node->is_dir ? (node->dir_count.load() + 1) : 0;
```
* **Explanation**: If successfully moved to trash, calculates the stats of the deleted item.

```cpp
481:             TreeNode* parentNode = node->parent;
```
* **Explanation**: Retrieves the parent node.

```cpp
483:             bool modelRemoved = m_treeModel->removeNode(index);
```
* **Explanation**: Removes the node row from the table model.

```cpp
484:             if (modelRemoved) {
485:                 if (parentNode) {
486:                     propagate_deletion_stats(parentNode, size_removed, allocated_removed, files_removed, dirs_removed);
```
* **Explanation**: Subtracts the deleted item's stats from all parent folders in the tree.

```cpp
489:                 m_scanner->adjust_stats(-static_cast<int64_t>(files_removed), -static_cast<int64_t>(dirs_removed), -static_cast<int64_t>(size_removed));
```
* **Explanation**: Subtracts the deleted item's stats from the global scanner counters.

```cpp
491:                 m_treeModel->layoutChanged();
492:                 updateStatsHUD(m_scanner->files_scanned(), m_scanner->dirs_scanned(), m_scanner->bytes_scanned());
493:             }
494:         } else {
495:             QMessageBox::critical(this, tr("Delete Failed"), tr("<font color='black'>Failed to move the item to trash. Please verify permissions or if the file is in use.</font>"));
496:         }
497:     }
498: }
```
* **Explanation**: Updates the layout, refreshes HUD statistics, and displays an error dialog if the trash operation fails.
