// ==============================================================================
// Program: space_map - file_tree_model.hpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Declares FileTreeModel, the Qt item model over TreeNode data, and PercentDelegate, which draws the percent-of-parent bar.
// ==============================================================================
#pragma once
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include "tree_node.hpp"

/// Qt tree model that exposes a TreeNode hierarchy to a QTreeView.
/// Columns are Name, % of Parent, Size, Allocated, Files and Folders.
class FileTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    /// Creates the model over an optional root node (not owned by the model).
    explicit FileTreeModel(TreeNode* root = nullptr, QObject* parent = nullptr);
    ~FileTreeModel() override = default;

    /// Swaps the root node and resets the view.
    void setRootNode(TreeNode* root);

    // QAbstractItemModel interface
    /// Builds the model index for a child row of the given parent.
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    /// Returns the index of the parent of the given item.
    QModelIndex parent(const QModelIndex& child) const override;
    /// Number of children under the given parent.
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    /// Number of columns (fixed at six).
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    /// Supplies display text, icons, alignment, and the raw percent value for each cell.
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    /// Supplies the column header titles.
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    /// Sorts every level of the tree by the chosen column and order.
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    /// Removes the node at the given index from the tree and notifies the view.
    bool removeNode(const QModelIndex& index);

    /// Converts a byte count to a short text such as "1.50 MB".
    // Helper to format byte sizes
    static QString formatSize(uint64_t bytes);


private:
    // Root of the displayed tree (not owned)
    TreeNode* m_root;
};

// Custom item delegate to draw a blue ratio bar inside '% of Parent' cells
/// Item delegate that paints a filled bar behind the percent text in column 1.
class PercentDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    /// Creates the delegate with an optional parent object.
    explicit PercentDelegate(QObject* parent = nullptr);
    /// Draws the cell, the bar proportional to the percentage, and centered text.
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
