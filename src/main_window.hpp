// ==============================================================================
// main_window.hpp - the application's main window (SpaceMap GUI).
//
// Holds the drive picker, path box, scan controls (scan/pause/cancel), the
// statistics HUD, the tree view over the scan results, an optional delete
// mode, and the theme customisation entry point. A QTimer refreshes the HUD
// while scanning so the worker threads never touch widgets directly.
// ==============================================================================
#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTreeView>
#include <QTimer>
#include <QSplitter>
#include <QProgressBar>
#include <QComboBox>
#include <QStorageInfo>
#include <QJsonArray>
#include <QJsonObject>
#include "scanner.hpp"
#include "file_tree_model.hpp"
#include "theme_manager.hpp"

/**
 * The application's main window. Owns the Scanner, the FileTreeModel and all
 * widgets; built entirely in code (no .ui file). See main_window.cpp for the
 * scan lifecycle.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /** Builds the UI and loads the saved theme; see the .cpp for the order of steps. */
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

// Slots are the handlers for button clicks, the scanner's finished() signal,
// the GUI timer, context menu requests, drive selection and theme changes.
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
    void onDriveSelected(int index);
    void refreshDrivesList();
    void onCustomizeLayoutClicked();
    void onThemeChanged();

private:
    void setupUi();
    // Applies the active theme's generated stylesheet (name predates themes).
    void applyDarkTheme();
    // Enable/disable buttons according to the scanner's running/paused state.
    void updateControls();
    void updateStatsHUD(uint64_t files, uint64_t dirs, uint64_t bytes);
    // Rebuilds a filesystem path by walking a node's parent chain.
    QString getAbsoluteNodePath(TreeNode* node);
    // Subtracts the removed amounts from every ancestor after a delete.
    void propagate_deletion_stats(TreeNode* start_node, uint64_t size_removed, uint64_t allocated_removed, uint64_t files_removed, uint64_t dirs_removed);
    // Linux drive discovery: walks the JSON tree produced by lsblk.
    void parseBlockDevices(const QJsonArray& devices);
    void processPartition(const QJsonObject& partObj);

    // Scanner & Data Model. The scanner owns the TreeNode tree; the model only
    // points at it (see FileTreeModel).
    // Scanner & Data Model
    Scanner* m_scanner = nullptr;
    FileTreeModel* m_treeModel = nullptr;

    // UI Widgets (all parented to this window, so Qt deletes them with it).
    // UI Widgets
    QComboBox* m_driveComboBox = nullptr;
    QPushButton* m_refreshDrivesBtn = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QPushButton* m_browseBtn = nullptr;
    QPushButton* m_scanBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_deleteToggleBtn = nullptr;
    QPushButton* m_customizeBtn = nullptr;
    // Delete shortcut action attached to the tree view; enabled only in delete mode.
    QAction* m_deleteAction = nullptr;


    // HUD (head-up display) statistics labels in the bottom bar.
    // HUD Stats
    QLabel* m_statusLabel = nullptr;
    QLabel* m_filesLabel = nullptr;
    QLabel* m_dirsLabel = nullptr;
    QLabel* m_capacityLabel = nullptr;
    QLabel* m_usedLabel = nullptr;
    QLabel* m_freeLabel = nullptr;


    // Results view and scan progress bar.
    // UI View Component
    QTreeView* m_treeView = nullptr;
    QProgressBar* m_progressBar = nullptr;

    // Fires every 200 ms during a scan (see onUpdateTimer).
    // GUI Refresh Timer
    QTimer* m_guiTimer = nullptr;
};
