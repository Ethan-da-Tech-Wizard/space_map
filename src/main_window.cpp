#include "main_window.hpp"
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


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_scanner = new Scanner(this);
    m_treeModel = new FileTreeModel(nullptr, this);

    m_guiTimer = new QTimer(this);
    connect(m_guiTimer, &QTimer::timeout, this, &MainWindow::onUpdateTimer);

    setMinimumSize(800, 600);
    setupUi();
    applyDarkTheme();
    updateControls();
}

void MainWindow::setupUi() {
    setWindowTitle("SpaceMap - C++ High-Speed Disk Analyzer");

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // ================= TOP BAR (SCAN CONTROLS) =================
    // A clean, structured 2-row layout to prevent squishing on smaller windows
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
    m_pathEdit->setText("/"); // Default path set to root directory for one-click full scan

    m_browseBtn = new QPushButton(tr("Browse..."), this);

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

    buttonBar->addWidget(m_scanBtn);
    buttonBar->addWidget(m_pauseBtn);
    buttonBar->addWidget(m_cancelBtn);
    buttonBar->addStretch(1); // Push the deletion toggle to the right
    buttonBar->addWidget(m_deleteToggleBtn);

    controlLayout->addLayout(buttonBar);

    mainLayout->addLayout(controlLayout);

    // ================= PROGRESS BAR (ACCURATE LOADING) =================
    m_progressBar = new QProgressBar(this);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(tr("Idle - 0%"));
    mainLayout->addWidget(m_progressBar);

    // ================= CENTRAL AREA (TREE VIEW) =================
    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_treeModel);
    m_treeView->setItemDelegateForColumn(1, new PercentDelegate(this));
    m_treeView->setAlternatingRowColors(true);
    m_treeView->setSortingEnabled(false); // Sorted by size internally when scan completes
    m_treeView->setIndentation(12); // Compact indentation to prevent clipping in deep paths
    m_treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_treeView->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_treeView->header()->setDefaultSectionSize(180);
    m_treeView->header()->setStretchLastSection(true);
    
    mainLayout->addWidget(m_treeView, 1); // Tree view occupies all available vertical space

    // ================= BOTTOM BAR (HUD STATS) =================
    QFrame* hudFrame = new QFrame(this);
    hudFrame->setObjectName("hudFrame");
    QHBoxLayout* hudLayout = new QHBoxLayout(hudFrame);
    hudLayout->setContentsMargins(15, 8, 15, 8);
    hudLayout->setSpacing(25);

    m_statusLabel = new QLabel(tr("Status: Idle"), this);
    m_filesLabel = new QLabel(tr("Files: 0"), this);
    m_dirsLabel = new QLabel(tr("Folders: 0"), this);
    m_bytesLabel = new QLabel(tr("Size: 0 B"), this);
    m_freeLabel = new QLabel(tr("Free Estate: 0 B"), this);

    hudLayout->addWidget(m_statusLabel);
    hudLayout->addWidget(m_filesLabel);
    hudLayout->addWidget(m_dirsLabel);
    hudLayout->addWidget(m_bytesLabel);
    hudLayout->addWidget(m_freeLabel);
    hudLayout->addStretch(1);

    mainLayout->addWidget(hudFrame);

    // Connect control events
    connect(m_driveComboBox, QOverload<int>::of(&QComboBox::activated), this, &MainWindow::onDriveSelected);
    connect(m_refreshDrivesBtn, &QPushButton::clicked, this, &MainWindow::refreshDrivesList);
    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseClicked);
    connect(m_scanBtn, &QPushButton::clicked, this, &MainWindow::onScanClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelClicked);
    connect(m_deleteToggleBtn, &QPushButton::toggled, this, &MainWindow::onDeleteToggleToggled);

    // Setup Custom Context Menu and Delete action shortcut on QTreeView
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::customContextMenuRequested, this, &MainWindow::onCustomContextMenuRequested);

    m_deleteAction = new QAction(tr("Delete to Trash"), this);
    m_deleteAction->setShortcut(QKeySequence::Delete);
    m_deleteAction->setShortcutContext(Qt::WidgetShortcut);
    m_deleteAction->setEnabled(false); // Disabled by default
    connect(m_deleteAction, &QAction::triggered, this, &MainWindow::onDeleteActionTriggered);
    m_treeView->addAction(m_deleteAction);

    // Auto-resize Column 0 on expand/collapse to handle deep folder clipping
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

    // Connect scanner finish
    connect(m_scanner, &Scanner::finished, this, &MainWindow::onScanFinished);

    // Initial drive listing
    refreshDrivesList();
}

void MainWindow::applyDarkTheme() {
    // Elegant dark mode theme styling
    QString stylesheet = R"(
        QMainWindow {
            background-color: #121212;
        }
        QLabel {
            color: #ffffff;
            font-size: 13px;
        }
        QProgressBar {
            background-color: #1a1a1a;
            color: #ffffff;
            border: 1px solid #2d2d2d;
            border-radius: 4px;
            text-align: center;
            height: 18px;
            font-size: 11px;
            font-weight: bold;
        }
        QProgressBar::chunk {
            background-color: #00d2ff;
            border-radius: 3px;
        }
        QLineEdit {
            background-color: #1e1e1e;
            color: #ffffff;
            border: 1px solid #333333;
            padding: 6px;
            border-radius: 4px;
            font-size: 13px;
        }
        QLineEdit:focus {
            border: 1px solid #007acc;
        }
        QComboBox {
            background-color: #1e1e1e;
            color: #ffffff;
            border: 1px solid #333333;
            padding: 6px 20px 6px 10px;
            border-radius: 4px;
            font-size: 13px;
            min-width: 180px;
        }
        QComboBox:focus {
            border: 1px solid #007acc;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left-width: 1px;
            border-left-color: #333333;
            border-left-style: solid;
            border-top-right-radius: 3px;
            border-bottom-right-radius: 3px;
        }
        QComboBox QAbstractItemView {
            background-color: #1e1e1e;
            color: #ffffff;
            border: 1px solid #333333;
            selection-background-color: #007acc;
            selection-color: #ffffff;
        }
        QPushButton {
            background-color: #2d2d2d;
            color: #e0e0e0;
            border: 1px solid #3a3a3a;
            padding: 6px 12px;
            border-radius: 4px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #3d3d3d;
            border: 1px solid #505050;
        }
        QPushButton:pressed {
            background-color: #005999;
        }
        QTreeView {
            background-color: #1a1a1a;
            color: #e0e0e0;
            border: 1px solid #2d2d2d;
            border-radius: 4px;
            alternate-background-color: #222222;
        }
        QTreeView::item:hover {
            background-color: #2a2a2a;
        }
        QTreeView::item:selected {
            background-color: #007acc;
            color: #ffffff;
        }
        QHeaderView::section {
            background-color: #252526;
            color: #d0d0d0;
            border: 1px solid #1a1a1a;
            padding: 4px;
        }
        #hudFrame {
            background-color: #1c1c1c;
            border: 1px solid #2d2d2d;
            border-radius: 4px;
        }
    )";
    setStyleSheet(stylesheet);
}

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

    if (run) {
        m_deleteToggleBtn->setEnabled(false);
        m_deleteAction->setEnabled(false);
    } else {
        m_deleteToggleBtn->setEnabled(true);
        m_deleteAction->setEnabled(m_deleteToggleBtn->isChecked());
    }
}

void MainWindow::updateStatsHUD(uint64_t files, uint64_t dirs, uint64_t bytes) {
    m_filesLabel->setText(QString("Files: %1").arg(files));
    m_dirsLabel->setText(QString("Folders: %1").arg(dirs));
    m_bytesLabel->setText(QString("Size: %1").arg(FileTreeModel::formatSize(bytes)));
    m_freeLabel->setText(QString("Free Estate: %1").arg(FileTreeModel::formatSize(m_scanner->free_bytes())));
}

void MainWindow::onBrowseClicked() {
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Directory to Scan"), m_pathEdit->text());
    if (!path.isEmpty()) {
        m_pathEdit->setText(path);
    }
}

void MainWindow::onScanClicked() {
    QString path = m_pathEdit->text();
    if (path.isEmpty()) return;

    m_treeView->setSortingEnabled(false);

    m_statusLabel->setText(tr("Status: Scanning..."));
    m_progressBar->setValue(0);
    m_progressBar->setFormat(tr("Scanning... %p%"));
    m_scanner->start(path.toStdString());
    
    m_treeModel->setRootNode(nullptr);

    m_guiTimer->start(200); // Refresh tree grid every 200ms
    updateControls();
}

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

void MainWindow::onCancelClicked() {
    m_statusLabel->setText(tr("Status: Cancelling..."));
    m_scanner->cancel();
    updateControls();
}

void MainWindow::onScanFinished() {
    m_guiTimer->stop();

    TreeNode* root = m_scanner->root_node();
    m_treeModel->setRootNode(root);

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

void MainWindow::onUpdateTimer() {
    // Inform model that data changed to update QTreeView in real-time
    m_treeModel->layoutChanged();

    // Update neon sky blue progress bar value
    m_progressBar->setValue(static_cast<int>(m_scanner->progress_percentage()));

    updateStatsHUD(m_scanner->files_scanned(), m_scanner->dirs_scanned(), m_scanner->bytes_scanned());
}

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
            m_deleteToggleBtn->setStyleSheet("background-color: #ff3333; color: white; border: 1px solid #ff6666; font-weight: bold;");
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

void MainWindow::onCustomContextMenuRequested(const QPoint& pos) {
    QModelIndex index = m_treeView->indexAt(pos);
    if (!index.isValid()) return;

    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu {
            background-color: #1e1e1e;
            color: #ffffff;
            border: 1px solid #333333;
        }
        QMenu::item:selected {
            background-color: #007acc;
            color: #ffffff;
        }
        QMenu::item:disabled {
            color: #666666;
        }
    )");

    QAction* deleteAct = new QAction(tr("Delete to Trash"), &menu);
    deleteAct->setEnabled(m_deleteToggleBtn->isChecked());
    connect(deleteAct, &QAction::triggered, this, &MainWindow::onDeleteActionTriggered);
    menu.addAction(deleteAct);

    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
}

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
    // Clean up double slashes
    size_t pos;
    while ((pos = path.find("//")) != std::string::npos) {
        path.replace(pos, 2, "/");
    }
    return QString::fromStdString(path);
}

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

    // Fall back to QStorageInfo if lsblk wasn't used or failed
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

            QString text = QString("%1 (%2) - %3 free of %4")
                .arg(name)
                .arg(rootPath)
                .arg(formatBytes(info.bytesAvailable()))
                .arg(formatBytes(info.bytesTotal()));

            m_driveComboBox->addItem(text, rootPath);
        }
    }

    // Set default selection (System Root /) if available
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
        QString text = QString("%1 (%2) - %3")
            .arg(displayName)
            .arg(mountpoint)
            .arg(size);
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

void MainWindow::onDriveSelected(int index) {
    if (index < 0 || index >= m_driveComboBox->count()) return;
    QString pathData = m_driveComboBox->itemData(index).toString();
    if (pathData.isEmpty()) return;

    if (pathData.startsWith("device:")) {
        QString devicePath = pathData.mid(7); // Extract raw path like "/dev/sda1"

        m_statusLabel->setText(tr("Status: Mounting %1...").arg(devicePath));
        qApp->processEvents();

        // 1. Try mounting normally via udisksctl
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

        // 2. If it fails, try read-only fallback mount (extremely useful for dirty NTFS logs)
        if (!success) {
            mountProc.start("udisksctl", QStringList() << "mount" << "-b" << devicePath << "-o" << "ro");
            if (mountProc.waitForFinished(8000)) {
                output = QString::fromUtf8(mountProc.readAllStandardOutput()).trimmed();
                errOutput = QString::fromUtf8(mountProc.readAllStandardError()).trimmed();
                success = (mountProc.exitCode() == 0);
            }
        }

        if (success) {
            // Find the mount point from the output, e.g. "Mounted /dev/sda1 at /run/media/ethan/54BE3B12BE3AEC5C"
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

        // Display detailed error dialog if mount failed completely
        QMessageBox::critical(this, tr("Mount Failed"), 
            tr("<font color='black'>Failed to mount device %1.<br><br><b>Error:</b><br>%2</font>")
                .arg(devicePath)
                .arg(errOutput.isEmpty() ? tr("Unknown mount failure.") : errOutput));
        
        refreshDrivesList();
    } else {
        m_pathEdit->setText(pathData);
    }
}

