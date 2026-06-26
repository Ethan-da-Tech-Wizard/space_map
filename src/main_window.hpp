#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTreeView>
#include <QTimer>
#include <QSplitter>
#include <QProgressBar>
#include "scanner.hpp"
#include "file_tree_model.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onBrowseClicked();
    void onScanClicked();
    void onPauseClicked();
    void onCancelClicked();
    void onScanFinished();
    void onUpdateTimer();
    void onDeleteToggleToggled(bool checked);
    void onDeleteActionTriggered();
    void onCustomContextMenuRequested(const QPoint& pos);

private:
    void setupUi();
    void applyDarkTheme();
    void updateControls();
    void updateStatsHUD(uint64_t files, uint64_t dirs, uint64_t bytes);
    QString getAbsoluteNodePath(TreeNode* node);
    void propagate_deletion_stats(TreeNode* start_node, uint64_t size_removed, uint64_t allocated_removed, uint64_t files_removed, uint64_t dirs_removed);

    // Scanner & Data Model
    Scanner* m_scanner = nullptr;
    FileTreeModel* m_treeModel = nullptr;

    // UI Widgets
    QLineEdit* m_pathEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;
    QPushButton* m_scanBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_deleteToggleBtn = nullptr;
    QAction* m_deleteAction = nullptr;


    // HUD Stats
    QLabel* m_statusLabel = nullptr;
    QLabel* m_filesLabel = nullptr;
    QLabel* m_dirsLabel = nullptr;
    QLabel* m_bytesLabel = nullptr;
    QLabel* m_freeLabel = nullptr;


    // UI View Component
    QTreeView* m_treeView = nullptr;
    QProgressBar* m_progressBar = nullptr;

    // GUI Refresh Timer
    QTimer* m_guiTimer = nullptr;
};
