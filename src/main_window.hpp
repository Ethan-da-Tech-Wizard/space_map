// ==============================================================================
// Program: space_map - main_window.hpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Declares MainWindow, the top-level window with scan controls, the results tree, drive list, and status display.
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

/// Main application window: lets the user pick a drive or folder, run and control a scan,
/// browse the results in a tree, optionally move items to the trash, and change the theme.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /// Builds the UI, loads the saved theme, and wires up the scanner and timer.
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

// Slots connected to buttons, menus, the scanner, and the theme manager
private slots:
    /// Opens a folder chooser and puts the result in the path box.
    void onBrowseClicked();
    /// Starts a scan of the path in the path box.
    void onScanClicked();
    /// Toggles between pausing and resuming the scan.
    void onPauseClicked();
    /// Cancels the running scan.
    void onCancelClicked();
    /// Shows the finished tree and updates status text.
    void onScanFinished();
    /// Periodic refresh while scanning (tree, progress bar, counters).
    void onUpdateTimer();
    /// Asks for confirmation before enabling deletion and updates the toggle's look.
    void onDeleteToggleToggled(bool checked);
    /// Moves the selected item to the trash after checks and confirmation, then updates totals.
    void onDeleteActionTriggered();
    /// Shows the right-click menu for the item under the cursor.
    void onCustomContextMenuRequested(const QPoint& pos);
    /// Applies a drive choice, mounting it first if it is listed as unmounted.
    void onDriveSelected(int index);
    /// Rebuilds the drive drop-down.
    void refreshDrivesList();
    /// Opens the theme customization dialog.
    void onCustomizeLayoutClicked();
    /// Re-applies the stylesheet when the active theme changes.
    void onThemeChanged();

private:
    /// Creates and arranges all widgets and connects their signals.
    void setupUi();
    /// Applies the active theme's stylesheet (the name is historical; any theme is applied).
    void applyDarkTheme();
    /// Enables or disables controls to match the scanner state.
    void updateControls();
    /// Refreshes the file, folder, capacity, used, and free labels.
    void updateStatsHUD(uint64_t files, uint64_t dirs, uint64_t bytes);
    /// Rebuilds a node's full filesystem path by walking up its parents.
    QString getAbsoluteNodePath(TreeNode* node);
    /// Subtracts removed totals from a node and all its ancestors, never going below zero.
    void propagate_deletion_stats(TreeNode* start_node, uint64_t size_removed, uint64_t allocated_removed, uint64_t files_removed, uint64_t dirs_removed);
    /// Walks lsblk JSON device entries recursively, handling leaf partitions.
    void parseBlockDevices(const QJsonArray& devices);
    /// Adds one partition to the drive drop-down, mounted or not.
    void processPartition(const QJsonObject& partObj);

    // Scanner & Data Model (owned through Qt parent-child ownership)
    // Scanner & Data Model
    Scanner* m_scanner = nullptr;
    FileTreeModel* m_treeModel = nullptr;

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
    QAction* m_deleteAction = nullptr;


    // HUD Stats
    QLabel* m_statusLabel = nullptr;
    QLabel* m_filesLabel = nullptr;
    QLabel* m_dirsLabel = nullptr;
    QLabel* m_capacityLabel = nullptr;
    QLabel* m_usedLabel = nullptr;
    QLabel* m_freeLabel = nullptr;


    // UI View Component
    QTreeView* m_treeView = nullptr;
    QProgressBar* m_progressBar = nullptr;

    // GUI Refresh Timer
    QTimer* m_guiTimer = nullptr;
};
