// ==============================================================================
// Program: main_window.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Main GUI window, layout styling, tree views, context menus, and trash tools
//
// How it fits: main() creates one MainWindow. It owns the Scanner (background
// crawler, scanner.cpp), the FileTreeModel (adapter for the results,
// file_tree_model.cpp) and all widgets, and styles itself from the singleton
// ThemeManager (theme_manager.cpp). The scanner's worker threads never touch
// widgets: they only update atomic counters, and a 200 ms QTimer on the GUI
// thread reads those counters to refresh the HUD and progress bar.
//
// Scan lifecycle: onScanClicked -> Scanner::start -> (timer ticks) ->
// Scanner::finished -> onScanFinished (hand the tree to the model, sort by
// size, enable deletion tools). Deleting moves the file to the trash and then
// fixes up the totals in memory; no rescan is needed.
//
// Linux extras: the drive list is built from `lsblk -J` (so unmounted
// partitions can be offered) and selecting one runs `udisksctl mount`.
//
// Known limits: the tree is only shown once the scan has finished (the model
// has no root while scanning); getAbsoluteNodePath assumes POSIX-style names.
// ==============================================================================
#include "main_window.hpp"
#include "theme_dialog.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QHeaderView>
#include <QApplication>
#include <QStyle>
#include <QDir>
#include <QPalette>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>


/**
 * Creates the scanner, model and refresh timer, builds the UI, loads the saved
 * theme and subscribes to theme changes. The model starts with no root; the
 * scan result is attached later by onScanFinished().
 * @param parent Optional parent widget.
 */
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_scanner = new Scanner(this);
    m_treeModel = new FileTreeModel(nullptr, this);

    // The timer is only started during a scan; see onScanClicked/onScanFinished.
    m_guiTimer = new QTimer(this);
    connect(m_guiTimer, &QTimer::timeout, this, &MainWindow::onUpdateTimer);

    // Below this size the two-row control area and the HUD would start to clip.
    setMinimumSize(800, 600);
    setupUi();

    // Theme must be loaded before the first stylesheet is generated; the
    // connection to themeChanged is made afterwards so loading does not trigger a second restyle.
    // Load saved theme or apply default
    ThemeManager::instance().loadSettings();
    applyDarkTheme();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &MainWindow::onThemeChanged);

    updateControls();
}

/**
 * Builds every widget and layout and wires the signals.
 * Layout, top to bottom: drive/path inputs and scan buttons, progress bar,
 * the results tree (takes all spare height) and a bottom HUD of statistics.
 */
void MainWindow::setupUi() {
    setWindowTitle("SpaceMap - C++ High-Speed Disk Analyzer");

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Top bar: two rows, so it does not squeeze on narrow windows.
    // ================= TOP BAR (SCAN CONTROLS) =================
    QVBoxLayout* controlLayout = new QVBoxLayout();
    controlLayout->setSpacing(8);

    // Row 1: Grid for Drive and Directory inputs
    QGridLayout* inputGrid = new QGridLayout();
    inputGrid->setHorizontalSpacing(8);
    inputGrid->setVerticalSpacing(6);

    QLabel* driveLabel = new QLabel(tr("Drive:"), this);
    m_driveComboBox = new QComboBox(this);
    m_refreshDrivesBtn = new QPushButton(tr("Refresh"), this);
    m_refreshDrivesBtn->setToolTip(tr("Refresh available drives"));

    QLabel* pathLabel = new QLabel(tr("Directory:"), this);
    m_pathEdit = new QLineEdit(this);
    // Default to the filesystem root so one click scans the whole disk.
    m_pathEdit->setText("/"); // Default path set to root directory for one-click full scan

    m_browseBtn = new QPushButton(tr("Browse..."), this);

    // Grid: column 0 labels, column 1 inputs (stretches), column 2 action buttons.
    inputGrid->addWidget(driveLabel, 0, 0);
    inputGrid->addWidget(m_driveComboBox, 0, 1);
    inputGrid->addWidget(m_refreshDrivesBtn, 0, 2);

    inputGrid->addWidget(pathLabel, 1, 0);
    inputGrid->addWidget(m_pathEdit, 1, 1);
    inputGrid->addWidget(m_browseBtn, 1, 2);

    // Allow combo box and text field to stretch to take available space
    inputGrid->setColumnStretch(1, 1);

    controlLayout->addLayout(inputGrid);

    // Row 2: Horizontal button bar for scan actions
    QHBoxLayout* buttonBar = new QHBoxLayout();
    buttonBar->setSpacing(8);

    m_scanBtn = new QPushButton(tr("Scan"), this);
    m_pauseBtn = new QPushButton(tr("Pause"), this);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_deleteToggleBtn = new QPushButton(tr("Enable Delete"), this);
    m_deleteToggleBtn->setCheckable(true);
    m_customizeBtn = new QPushButton(tr("Customize Layout"), this);

    buttonBar->addWidget(m_scanBtn);
    buttonBar->addWidget(m_pauseBtn);
    buttonBar->addWidget(m_cancelBtn);
    // Spacer pushes the layout-customisation and delete-mode buttons to the right edge.
    buttonBar->addStretch(1); // Push the right-side buttons to the right
    buttonBar->addWidget(m_customizeBtn);
    buttonBar->addWidget(m_deleteToggleBtn);

    controlLayout->addLayout(buttonBar);

    mainLayout->addLayout(controlLayout);

    // Progress bar: shows scanner.progress_percentage() while scanning (see onUpdateTimer).
    // ================= PROGRESS BAR (ACCURATE LOADING) =================
    m_progressBar = new QProgressBar(this);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(tr("Idle - 0%"));
    mainLayout->addWidget(m_progressBar);

    // Results tree. Column 1 is drawn by PercentDelegate (a bar), the others
    // use the model's text. Interactive resize lets users widen columns.
    // ================= CENTRAL AREA (TREE VIEW) =================
    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_treeModel);
    m_treeView->setItemDelegateForColumn(1, new PercentDelegate(this));
    m_treeView->setAlternatingRowColors(true);
    // Sorting is off while scanning (the tree is changing); it is turned on in
    // onScanFinished once the data is complete.
    m_treeView->setSortingEnabled(false); // Sorted by size internally when scan completes
    m_treeView->setIndentation(12); // Compact indentation to prevent clipping in deep paths
    m_treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_treeView->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_treeView->header()->setDefaultSectionSize(180);
    m_treeView->header()->setStretchLastSection(true);
    
    mainLayout->addWidget(m_treeView, 1); // Tree view occupies all available vertical space

    // HUD (head-up display) strip at the bottom: object name "hudFrame" is the
    // hook the theme stylesheet uses to style it.
    // ================= BOTTOM BAR (HUD STATS) =================
    QFrame* hudFrame = new QFrame(this);
    hudFrame->setObjectName("hudFrame");
    QHBoxLayout* hudLayout = new QHBoxLayout(hudFrame);
    hudLayout->setContentsMargins(15, 8, 15, 8);
    hudLayout->setSpacing(25);

    m_statusLabel = new QLabel(tr("Status: Idle"), this);
    m_filesLabel = new QLabel(tr("Files: 0"), this);
    m_dirsLabel = new QLabel(tr("Folders: 0"), this);
    m_capacityLabel = new QLabel(tr("Total Size: 0 B"), this);
    m_usedLabel = new QLabel(tr("Used Space: 0 B (0.0%)"), this);
    m_freeLabel = new QLabel(tr("Free Space: 0 B (100.0%)"), this);

    hudLayout->addWidget(m_statusLabel);
    hudLayout->addWidget(m_filesLabel);
    hudLayout->addWidget(m_dirsLabel);
    hudLayout->addWidget(m_capacityLabel);
    hudLayout->addWidget(m_usedLabel);
    hudLayout->addWidget(m_freeLabel);
    hudLayout->addStretch(1);

    mainLayout->addWidget(hudFrame);

    // Wire the controls; the scanner's own signal is connected at the end.
    // Connect control events
    connect(m_driveComboBox, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::onDriveSelected);
    connect(m_refreshDrivesBtn, &QPushButton::clicked, this, &MainWindow::refreshDrivesList);
    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(m_scanBtn, &QPushButton::clicked, this, &MainWindow::onScanClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelClicked);
    connect(m_deleteToggleBtn, &QPushButton::toggled, this, &MainWindow::onDeleteToggleToggled);
    connect(m_customizeBtn, &QPushButton::clicked, this, &MainWindow::onCustomizeLayoutClicked);

    // The Delete shortcut is bound to the tree view only (WidgetShortcut) so it
    // cannot fire while typing in the path box, and stays disabled until the
    // user explicitly enables deletion.
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onCustomContextMenuRequested);

    m_deleteAction = new QAction(tr("Delete to Trash"), this);
    m_deleteAction->setShortcut(QKeySequence::Delete);
    m_deleteAction->setShortcutContext(Qt::WidgetShortcut);
    m_deleteAction->setEnabled(false); // Disabled by default
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::onDeleteActionTriggered);
    m_treeView->addAction(m_deleteAction);

    // Column 0 holds the deeply indented names, so after every expand/collapse
    // it is re-fitted. The call is deferred with a zero-delay timer so it runs
    // after the view has finished expanding/collapsing and new row widths are known.
    connect(m_treeView, &QTreeView::expanded, this, [this](const QModelIndex&) {
        QTimer::singleShot(0, this, [this]() {
            m_treeView->resizeColumnToContents(0);
        });
    });
    connect(m_treeView, &QTreeView::collapsed, this, [this](const QModelIndex&) {
        QTimer::singleShot(0, this, [this]() {
            m_treeView->resizeColumnToContents(0);
        });
    });

    // finished() is emitted from a worker thread when the scan completes on its own; Qt then queues the call onto the GUI thread.
    // Connect scanner finish
    connect(m_scanner, &Scanner::finished, this, &MainWindow::onScanFinished);

    // Populate the drive combo box once at start-up.
    // Initial drive listing
    refreshDrivesList();
}

/** Regenerates and applies the stylesheet for the active theme (the name predates theming). */
void MainWindow::applyDarkTheme() {
    // Apply theme from ThemeManager
    const Theme& t = ThemeManager::instance().activeTheme();
    setStyleSheet(ThemeManager::instance().generateStylesheet(t));
}

/**
 * Derives button states from the scanner state in ONE place: inputs and Scan
 * are available only when idle, Pause (relabelled Resume) and Cancel only
 * while running, and deletion is locked out during a scan.
 */
void MainWindow::updateControls() {
    bool run = m_scanner->running();
    bool pause = m_scanner->paused();

    m_driveComboBox->setEnabled(!run);
    m_refreshDrivesBtn->setEnabled(!run);
    m_pathEdit->setEnabled(!run);
    m_browseBtn->setEnabled(!run);
    m_scanBtn->setEnabled(!run);
    
    m_pauseBtn->setEnabled(run);
    m_pauseBtn->setText(pause ? tr("Resume") : tr("Pause"));
    
    m_cancelBtn->setEnabled(run);

    // Deleting while a scan is writing the tree would race; the toggle is
    // forced off-limits until the scan ends, then restored to the user's choice.
    if (run) {
        m_deleteToggleBtn->setEnabled(false);
        m_deleteAction->setEnabled(false);
    } else {
        m_deleteToggleBtn->setEnabled(true);
        m_deleteAction->setEnabled(m_deleteToggleBtn->isChecked());
    }
}

/**
 * Updates the Files/Folders/Used/Free labels.
 * Capacity and free space come from the volume (statvfs), so "used" includes
 * data outside the scanned folder; only if those figures are unavailable is
 * the scanned byte count used. The free percentage is computed as 100 - used
 * so the two always add up to exactly 100.0 % after rounding.
 * @param files Files counted so far.
 * @param dirs  Folders counted so far.
 * @param bytes Logical bytes counted so far.
 */
void MainWindow::updateStatsHUD(uint64_t files, uint64_t dirs, uint64_t bytes) {
    m_filesLabel->setText(QString("Files: %1").arg(QLocale::system().toString(static_cast<qlonglong>(files))));
    m_dirsLabel->setText(QString("Folders: %1").arg(QLocale::system().toString(static_cast<qlonglong>(dirs))));

    uint64_t total_capacity = m_scanner->total_capacity_bytes();
    uint64_t free_space = m_scanner->free_bytes();
    // If free > capacity (odd filesystems) fall back to the scanned bytes
    // instead of letting the subtraction wrap around (unsigned).
    uint64_t used_space = total_capacity >= free_space ? (total_capacity - free_space) : bytes;

    double pct_used = 0.0;
    double pct_free = 0.0;

    if (total_capacity > 0) {
        double raw_used_pct = (static_cast<double>(used_space) / static_cast<double>(total_capacity)) * 100.0;
        // Round to one decimal first so the displayed numbers add up.
        pct_used = std::round(raw_used_pct * 10.0) / 10.0;
        if (pct_used > 100.0) pct_used = 100.0;
        if (pct_used < 0.0) pct_used = 0.0;
        pct_free = 100.0 - pct_used; // Mathematical complement guarantee: pct_used + pct_free == 100.0%
    }

    m_capacityLabel->setText(QString("Total Size: %1").arg(FileTreeModel::formatSize(total_capacity)));
    m_usedLabel->setText(QString("Used Space: %1 (%2%)")
        .arg(FileTreeModel::formatSize(used_space))
        .arg(QString::number(pct_used, 'f', 1)));
    m_freeLabel->setText(QString("Free Space: %1 (%2%)")
        .arg(FileTreeModel::formatSize(free_space))
        .arg(QString::number(pct_free, 'f', 1)));
}

/** Opens a folder picker starting at the current path and copies the choice into the path box. */
void MainWindow::onBrowseClicked() {
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Directory to Scan"), m_pathEdit->text());
    if (!path.isEmpty()) {
        m_pathEdit->setText(path);
    }
}

/**
 * Begins a scan of the path in the box.
 * Scanner::start() creates the new tree (and frees the old one) so the model's
 * root is cleared right after, otherwise the view would hold pointers to freed
 * nodes. The 200 ms timer then refreshes the HUD until finished() arrives.
 */
void MainWindow::onScanClicked() {
    QString path = m_pathEdit->text();
    if (path.isEmpty()) return;

    m_treeView->setSortingEnabled(false);
// Disable sorting before data changes; it is re-enabled when the scan ends.

    m_statusLabel->setText(tr("Status: Scanning..."));
    m_progressBar->setValue(0);
    m_progressBar->setFormat(tr("Scanning... %p%"));
    m_scanner->start(path.toStdString());
    
    m_treeModel->setRootNode(nullptr);

    m_guiTimer->start(200); // Refresh tree grid every 200ms
    updateControls();
}

/** Toggles between pause and resume; the button label follows via updateControls(). */
void MainWindow::onPauseClicked() {
    if (m_scanner->paused()) {
        m_statusLabel->setText(tr("Status: Scanning..."));
        m_scanner->resume();
    } else {
        m_statusLabel->setText(tr("Status: Paused"));
        m_scanner->pause();
    }
    updateControls();
}

/** Requests cancellation; the scanner then emits finished(), which runs onScanFinished(). */
void MainWindow::onCancelClicked() {
    m_statusLabel->setText(tr("Status: Cancelling..."));
    m_scanner->cancel();
    updateControls();
}

/**
 * Scan ended (complete, cancelled or empty): stops the timer, hands the
 * finished tree to the model, sorts by size descending, collapses to the top
 * level, sizes the columns and sets the status text for the outcome.
 */
void MainWindow::onScanFinished() {
    m_guiTimer->stop();

    TreeNode* root = m_scanner->root_node();
    m_treeModel->setRootNode(root);

    // Setting the indicator first means enabling sorting sorts exactly once
    // (size, descending) rather than by the previous indicator and again.
    // Set sort indicator first to prevent redundant sorting when enabling sorting
    m_treeView->header()->setSortIndicator(2, Qt::DescendingOrder);
    m_treeView->setSortingEnabled(true);

    // Collapse all items to show the clean base-level view
    m_treeView->collapseAll();

    // Dynamically resize all columns to contents initially
    for (int i = 0; i < 6; ++i) {
        m_treeView->resizeColumnToContents(i);
    }

    // 4. Update HUD and Progress Bar
    if (m_scanner->cancelled()) {
        m_statusLabel->setText(tr("Status: Cancelled"));
        m_progressBar->setFormat(tr("Cancelled - %p%"));
    } else if (m_scanner->files_scanned() == 0 && m_scanner->dirs_scanned() == 0) {
        m_statusLabel->setText(tr("Status: Failed/Empty"));
        m_progressBar->setFormat(tr("Failed/Empty"));
    } else {
        m_statusLabel->setText(tr("Status: Complete"));
        m_progressBar->setValue(100);
        m_progressBar->setFormat(tr("Complete - 100%"));
    }
    updateStatsHUD(m_scanner->files_scanned(), m_scanner->dirs_scanned(), m_scanner->bytes_scanned());
    updateControls();
}

/**
 * Timer slot (GUI thread, every 200 ms while scanning): tells the view the
 * data changed and refreshes progress and counters from the scanner's atomics.
 */
void MainWindow::onUpdateTimer() {
    m_treeModel->layoutChanged();

    // Update neon sky blue progress bar value
    m_progressBar->setValue(static_cast<int>(m_scanner->progress_percentage()));

    updateStatsHUD(m_scanner->files_scanned(), m_scanner->dirs_scanned(), m_scanner->bytes_scanned());
}

/**
 * Delete-mode switch. Turning it on shows a warning dialog defaulting to
 * "No"; declining unchecks the button again without re-triggering this slot
 * (its signals are blocked while resetting it).
 * @param checked New state of the toggle button.
 */
void MainWindow::onDeleteToggleToggled(bool checked) {
    if (checked) {
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this,
            tr("Enable File Deletion"),
            tr("<font color='black'>Are you sure you wish to enable file deletion?<br>This allows you to permanently move files and folders to the recycle bin. Please use with caution.</font>"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (reply == QMessageBox::Yes) {
            m_deleteAction->setEnabled(true);
            const Theme& t = ThemeManager::instance().activeTheme();
            m_deleteToggleBtn->setStyleSheet(QString("background-color: %1; color: white; border: 1px solid %2; font-weight: bold;")
                .arg(t.deleteEnabledBg.name())
                .arg(t.deleteEnabledBorder.name()));
            m_deleteToggleBtn->setText(tr("Deletion Enabled"));
        } else {
            m_deleteToggleBtn->blockSignals(true);
            m_deleteToggleBtn->setChecked(false);
            m_deleteToggleBtn->blockSignals(false);
            m_deleteAction->setEnabled(false);
        }
    } else {
        m_deleteAction->setEnabled(false);
        m_deleteToggleBtn->setStyleSheet("");
        m_deleteToggleBtn->setText(tr("Enable Delete"));
    }
}

/**
 * Right-click menu with a single "Delete to Trash" entry, enabled only while
 * deletion mode is on; styled from the active theme.
 * @param pos Click position in the tree viewport's coordinates.
 */
void MainWindow::onCustomContextMenuRequested(const QPoint& pos) {
    QModelIndex index = m_treeView->indexAt(pos);
    if (!index.isValid()) return;

    QMenu menu(this);
    const Theme& t = ThemeManager::instance().activeTheme();
    // QMenu is a top-level popup that does not inherit the main stylesheet's
    // look reliably, so it gets its own theme-derived stylesheet.
    menu.setStyleSheet(QString(R"(
        QMenu {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
        }
        QMenu::item:selected {
            background-color: %4;
            color: %5;
        }
        QMenu::item:disabled {
            color: #666666;
        }
    )")
    .arg(t.menuBg.name())
    .arg(t.menuText.name())
    .arg(t.menuBorder.name())
    .arg(t.menuSelectedBg.name())
    .arg(t.menuText.name()));

    QAction* deleteAct = new QAction(tr("Delete to Trash"), &menu);
    deleteAct->setEnabled(m_deleteToggleBtn->isChecked());
    connect(deleteAct, &QAction::triggered, this, &MainWindow::onDeleteActionTriggered);
    menu.addAction(deleteAct);

    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

/**
 * Rebuilds the full filesystem path of a node by joining names from the node
 * up to the root. The root node's name IS the scanned path, so a "/" root
 * contributes just a leading slash.
 * @return Absolute path, or an empty string for a null node.
 */
QString MainWindow::getAbsoluteNodePath(TreeNode* node) {
    if (!node) return QString();
    std::string path = node->name;
    TreeNode* curr = node->parent;
    while (curr) {
        if (curr->name == "/") {
            path = "/" + path;
        } else {
            path = curr->name + "/" + path;
        }
        curr = curr->parent;
    }
    // Joining a root such as "/home/" with names can produce "//"; collapse them.
    size_t pos;
    while ((pos = path.find("//")) != std::string::npos) {
        path.replace(pos, 2, "/");
    }
    return QString::fromStdString(path);
}

/**
 * Subtracts the removed amounts from @p start_node and every ancestor after a
 * delete. Each counter is clamped at zero so a mismatch can never underflow
 * the unsigned values and show enormous sizes.
 */
void MainWindow::propagate_deletion_stats(TreeNode* start_node, uint64_t size_removed, uint64_t allocated_removed, uint64_t files_removed, uint64_t dirs_removed) {
    TreeNode* current = start_node;
    while (current != nullptr) {
        if (current->size >= size_removed) current->size -= size_removed;
        else current->size = 0;

        if (current->allocated_size >= allocated_removed) current->allocated_size -= allocated_removed;
        else current->allocated_size = 0;

        if (current->file_count >= files_removed) current->file_count -= files_removed;
        else current->file_count = 0;

        if (current->dir_count >= dirs_removed) current->dir_count -= dirs_removed;
        else current->dir_count = 0;

        current = current->parent;
    }
}

/**
 * Delete selected item to the trash (guarded).
 * Refuses: during a scan, for the root folder itself, when deletion is not
 * enabled, if the path vanished, or if it is a symlink. Asks for confirmation
 * (default No). Items go to the Trash (QFile::moveToTrash), never permanently
 * deleted. On success the node is removed from the model and the totals of all
 * ancestors and the scanner's counters are reduced by the removed amounts.
 */
void MainWindow::onDeleteActionTriggered() {
    if (m_scanner->running()) {
        QMessageBox::warning(this, tr("Delete Error"), tr("<font color='black'>Cannot delete files while scanning is active. Please wait for the scan to finish or cancel it first.</font>"));
        return;
    }

    QModelIndex index = m_treeView->currentIndex();
    if (!index.isValid()) return;

    TreeNode* node = static_cast<TreeNode*>(index.internalPointer());
    if (!node) return;

    if (!node->parent) {
        QMessageBox::warning(this, tr("Delete Error"), tr("<font color='black'>Cannot delete the root scanned folder itself.</font>"));
        return;
    }

    if (!m_deleteToggleBtn->isChecked()) {
        QMessageBox::warning(this, tr("Delete Disabled"), tr("<font color='black'>File deletion is disabled. Please toggle 'Enable Delete' first.</font>"));
        return;
    }

    QString absolutePath = getAbsoluteNodePath(node);
    if (absolutePath.isEmpty()) return;

    QFileInfo fileInfo(absolutePath);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, tr("Delete Error"), tr("<font color='black'>File no longer exists.</font>"));
        return;
    }
    if (fileInfo.isSymLink()) {
        QMessageBox::warning(this, tr("Delete Error"), tr("<font color='black'>Security Alert: Symbolic link detected. Deletion aborted to prevent safety issues.</font>"));
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        // Capture the numbers BEFORE removeNode(): erasing the node destroys it.
        // A deleted directory also removes itself, hence dir_count + 1.
        this,
        tr("Confirm Delete"),
        tr("<font color='black'>Are you sure you wish to delete this %1?<br>Path: %2<br><br>It will be moved to the recycle bin/trash.</font>")
            .arg(node->is_dir ? tr("folder") : tr("file"))
            .arg(absolutePath),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        bool success = QFile::moveToTrash(absolutePath);
        if (success) {
            uint64_t size_removed = node->size;
            uint64_t allocated_removed = node->allocated_size;
            uint64_t files_removed = node->is_dir ? node->file_count.load() : 1;
            uint64_t dirs_removed = node->is_dir ? (node->dir_count.load() + 1) : 0;

            TreeNode* parentNode = node->parent;

            bool modelRemoved = m_treeModel->removeNode(index);
            if (modelRemoved) {
                if (parentNode) {
                    propagate_deletion_stats(parentNode, size_removed, allocated_removed, files_removed, dirs_removed);
                }

                m_scanner->adjust_stats(-static_cast<int64_t>(files_removed), -static_cast<int64_t>(dirs_removed), -static_cast<int64_t>(size_removed));

                m_treeModel->layoutChanged();
                updateStatsHUD(m_scanner->files_scanned(), m_scanner->dirs_scanned(), m_scanner->bytes_scanned());
            }
        } else {
            QMessageBox::critical(this, tr("Delete Failed"), tr("<font color='black'>Failed to move the item to trash. Please verify permissions or if the file is in use.</font>"));
        }
    }
}

/**
 * Fills the drive combo box. On Linux it first tries `lsblk -J` (which also
 * lists unmounted partitions); otherwise (or if that fails) it falls back to
 * QStorageInfo's mounted volumes, skipping virtual filesystems. Signals are
 * blocked while repopulating so this does not trigger onDriveSelected.
 * Item data: the mount point, or "device:<path>" for unmounted partitions.
 */
void MainWindow::refreshDrivesList() {
    m_driveComboBox->blockSignals(true);
    m_driveComboBox->clear();

    bool parsedWithLsblk = false;

#ifdef __linux__
    // Try running lsblk to fetch both mounted and unmounted block devices
    QProcess process;
    process.start("lsblk", QStringList() << "-p" << "-o" << "NAME,FSTYPE,SIZE,MOUNTPOINT,LABEL,UUID" << "-J");
    if (process.waitForFinished(3000)) {
        QByteArray output = process.readAllStandardOutput();
        QJsonDocument doc = QJsonDocument::fromJson(output);
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject rootObj = doc.object();
            if (rootObj.contains("blockdevices")) {
                QJsonArray devices = rootObj["blockdevices"].toArray();
                parseBlockDevices(devices);
                parsedWithLsblk = true;
            }
        }
    }
#endif

    // Fallback: also used on macOS and Windows, where lsblk does not exist.
    if (!parsedWithLsblk) {
        QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();
        
        auto formatBytes = [](qint64 bytes) -> QString {
            double kb = bytes / 1024.0;
            double mb = kb / 1024.0;
            double gb = mb / 1024.0;
            double tb = gb / 1024.0;

            if (tb >= 1.0) return QString("%1 TB").arg(tb, 0, 'f', 1);
            if (gb >= 1.0) return QString("%1 GB").arg(gb, 0, 'f', 1);
            if (mb >= 1.0) return QString("%1 MB").arg(mb, 0, 'f', 1);
            if (kb >= 1.0) return QString("%1 KB").arg(kb, 0, 'f', 1);
            return QString("%1 B").arg(bytes);
        };

        for (const QStorageInfo& info : volumes) {
            if (!info.isValid() || !info.isReady()) continue;
            if (info.bytesTotal() <= 0) continue;

            QString rootPath = info.rootPath();
            QString fsType = info.fileSystemType().toLower();

            if (fsType == "tmpfs" || fsType == "devtmpfs" || fsType == "sysfs" || 
                fsType == "proc" || fsType == "squashfs" || fsType == "cgroup") {
                continue;
            }

            if (rootPath == "/proc" || rootPath == "/sys" || rootPath == "/dev" || rootPath == "/run" || rootPath == "/sys/fs/cgroup") {
                continue;
            }

            QString name = info.displayName();
            if (name.isEmpty() || name == rootPath) {
                if (rootPath == "/") {
                    name = tr("System Root");
                } else {
                    QDir dir(rootPath);
                    name = dir.dirName();
                    if (name.isEmpty()) {
                        name = tr("Drive");
                    }
                }
            }

            QString text = QString("%1 (%2) - %3")
                .arg(name)
                .arg(rootPath)
                .arg(formatBytes(info.bytesTotal()));

            m_driveComboBox->addItem(text, rootPath);
        }
    }

    // Prefer "/" as the default drive, otherwise the first entry.
    int rootIndex = -1;
    for (int i = 0; i < m_driveComboBox->count(); ++i) {
        QString data = m_driveComboBox->itemData(i).toString();
        if (data == "/") {
            rootIndex = i;
            break;
        }
    }

    if (rootIndex != -1) {
        m_driveComboBox->setCurrentIndex(rootIndex);
        m_pathEdit->setText("/");
    } else if (m_driveComboBox->count() > 0) {
        m_driveComboBox->setCurrentIndex(0);
        QString firstData = m_driveComboBox->itemData(0).toString();
        if (firstData.startsWith("device:")) {
            m_pathEdit->setText(firstData.mid(7));
        } else {
            m_pathEdit->setText(firstData);
        }
    }

    m_driveComboBox->blockSignals(false);
}

/** Recursively visits lsblk's JSON; only leaf devices (no children) are treated as partitions. */
void MainWindow::parseBlockDevices(const QJsonArray& devices) {
    for (const QJsonValue& devVal : devices) {
        QJsonObject devObj = devVal.toObject();
        if (devObj.contains("children")) {
            QJsonArray children = devObj["children"].toArray();
            parseBlockDevices(children); // recursively parse partitions/children
        } else {
            processPartition(devObj);
        }
    }
}

/**
 * Adds one lsblk entry to the combo box: skips unformatted and virtual
 * filesystems; mounted ones carry their mount point as item data, unmounted
 * ones are marked "device:<path>" so selecting them triggers a mount attempt.
 */
void MainWindow::processPartition(const QJsonObject& partObj) {
    QString name = partObj["name"].toString();
    QString fstype = partObj["fstype"].toString();
    QString mountpoint = partObj["mountpoint"].toString();
    QString size = partObj["size"].toString();
    QString label = partObj["label"].toString();

    // Skip partitions with no file system format (like swap, unformatted partition tables)
    if (fstype.isEmpty()) return;

    // Filter out typical virtual/special loop filesystems
    if (fstype == "tmpfs" || fstype == "devtmpfs" || fstype == "sysfs" || 
        fstype == "proc" || fstype == "squashfs" || fstype == "cgroup") {
        return;
    }

    if (!mountpoint.isEmpty()) {
        // Skip root/system mounting configurations that are not storage-related
        if (mountpoint == "/proc" || mountpoint == "/sys" || mountpoint == "/dev" || mountpoint == "/run" || mountpoint == "/sys/fs/cgroup") {
            return;
        }

        QString displayName = label.isEmpty() ? name : label;
        QStorageInfo storage(mountpoint);
        QString totalSizeStr = size;
        if (storage.isValid() && storage.isReady() && storage.bytesTotal() > 0) {
            totalSizeStr = FileTreeModel::formatSize(storage.bytesTotal());
        }

        QString text = QString("%1 (%2) - %3")
            .arg(displayName)
            .arg(mountpoint)
            .arg(totalSizeStr);
        m_driveComboBox->addItem(text, mountpoint);
    } else {
        // Unmounted drive! Show "Click to Mount" option
        QString displayName = label.isEmpty() ? name : label;
        QString text = QString("%1 (%2) - %3 [Unmounted - Click to Mount]")
            .arg(displayName)
            .arg(name)
            .arg(size);
        m_driveComboBox->addItem(text, "device:" + name);
    }
}

/**
 * Slot for the drive combo box. Selecting a mounted drive fills the path box.
 * Selecting an unmounted device tries `udisksctl mount`, then a read-only
 * retry, and refreshes the list. Blocks the GUI up to 8 s per attempt.
 * @param index Combo box row.
 */
void MainWindow::onDriveSelected(int index) {
    if (index < 0 || index >= m_driveComboBox->count()) return;
    QString pathData = m_driveComboBox->itemData(index).toString();
    if (pathData.isEmpty()) return;

    if (pathData.startsWith("device:")) {
        QString devicePath = pathData.mid(7); // Extract raw path like "/dev/sda1"

        m_statusLabel->setText(tr("Status: Mounting %1...").arg(devicePath));
        qApp->processEvents();

        // 1. Normal mount (udisksctl needs no root for removable and internal drives
        // on typical desktops via polkit).
        QProcess mountProc;
        mountProc.start("udisksctl", QStringList() << "mount" << "-b" << devicePath);
        if (!mountProc.waitForFinished(8000)) {
            QMessageBox::critical(this, tr("Mount Timeout"), tr("<font color='black'>Mounting device %1 timed out.</font>").arg(devicePath));
            refreshDrivesList();
            return;
        }

        QString output = QString::fromUtf8(mountProc.readAllStandardOutput()).trimmed();
        QString errOutput = QString::fromUtf8(mountProc.readAllStandardError()).trimmed();
        bool success = (mountProc.exitCode() == 0);

        // 2. Read-only retry: a dirty NTFS volume refuses a normal mount.
        if (!success) {
            mountProc.start("udisksctl", QStringList() << "mount" << "-b" << devicePath << "-o" << "ro");
            if (mountProc.waitForFinished(8000)) {
                output = QString::fromUtf8(mountProc.readAllStandardOutput()).trimmed();
                errOutput = QString::fromUtf8(mountProc.readAllStandardError()).trimmed();
                success = (mountProc.exitCode() == 0);
            }
        }

        // udisksctl prints "Mounted /dev/sda1 at /run/media/user/LABEL"; the path
        // after " at " is the new mount point.
        if (success) {
            int atIndex = output.indexOf(" at ");
            if (atIndex != -1) {
                QString mountPath = output.mid(atIndex + 4).trimmed();
                m_pathEdit->setText(mountPath);
                
                // Refresh list so the drive now appears as mounted
                refreshDrivesList();

                // Select the newly mounted path in the combobox
                for (int i = 0; i < m_driveComboBox->count(); ++i) {
                    if (m_driveComboBox->itemData(i).toString() == mountPath) {
                        m_driveComboBox->setCurrentIndex(i);
                        break;
                    }
                }

                m_statusLabel->setText(tr("Status: Mounted at %1").arg(mountPath));
                return;
            }
        }

        QMessageBox::critical(this, tr("Mount Failed"), 
            tr("<font color='black'>Failed to mount device %1.<br><br><b>Error:</b><br>%2</font>")
                .arg(devicePath)
                .arg(errOutput.isEmpty() ? tr("Unknown mount failure.") : errOutput));
        
        refreshDrivesList();
    } else {
        m_pathEdit->setText(pathData);
    }
}

/** Opens the modal theme editor (ThemeDialog); changes arrive through ThemeManager::themeChanged. */
void MainWindow::onCustomizeLayoutClicked() {
    ThemeDialog dialog(this);
    dialog.exec();
}

/** Re-applies the stylesheet when the theme changes. */
void MainWindow::onThemeChanged() {
    applyDarkTheme();
}
