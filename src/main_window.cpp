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


MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_scanner = new Scanner(this);
    m_treeModel = new FileTreeModel(nullptr, this);

    m_guiTimer = new QTimer(this);
    connect(m_guiTimer, &QTimer::timeout, this, &MainWindow::onUpdateTimer);

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
    QHBoxLayout* topBar = new QHBoxLayout();
    topBar->setSpacing(8);

    QLabel* pathLabel = new QLabel(tr("Directory:"), this);
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setText("/"); // Default path set to root directory for one-click full scan

    m_browseBtn = new QPushButton(tr("Browse..."), this);
    m_scanBtn = new QPushButton(tr("Scan"), this);
    m_pauseBtn = new QPushButton(tr("Pause"), this);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_deleteToggleBtn = new QPushButton(tr("Enable Delete"), this);
    m_deleteToggleBtn->setCheckable(true);

    topBar->addWidget(pathLabel);
    topBar->addWidget(m_pathEdit, 1);
    topBar->addWidget(m_browseBtn);
    topBar->addWidget(m_scanBtn);
    topBar->addWidget(m_pauseBtn);
    topBar->addWidget(m_cancelBtn);
    topBar->addWidget(m_deleteToggleBtn);

    mainLayout->addLayout(topBar);

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

