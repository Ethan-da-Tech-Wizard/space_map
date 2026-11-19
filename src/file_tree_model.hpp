// ==============================================================================
// file_tree_model.hpp - Qt model/view adapter for the scanned TreeNode tree.
//
// FileTreeModel exposes a TreeNode hierarchy to QTreeView as a six-column
// table (Name, % of Parent, Size, Allocated, Files, Folders). QModelIndex
// internal pointers are TreeNode pointers, so nodes must outlive any index
// that refers to them. PercentDelegate draws the "% of Parent" bar.
// ==============================================================================
#pragma once
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include "tree_node.hpp"

class FileTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit FileTreeModel(TreeNode* root = nullptr, QObject* parent = nullptr);
    ~FileTreeModel() override = default;

    // Swap in a new tree (or nullptr) and reset every view attached to the model.
    void setRootNode(TreeNode* root);

    // Standard model overrides; see the .cpp for the locking each one uses.
    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    // Sorts every level of the tree by the chosen column; resets the model.
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    // Removes one node (and its subtree) from the model only; sizes held by the
    // ancestors are NOT adjusted here (MainWindow does that after deleting on disk).
    bool removeNode(const QModelIndex& index);

    // Helper to format byte sizes
    static QString formatSize(uint64_t bytes);


private:
    // Not owned; the Scanner owns the tree.
    TreeNode* m_root;
};

// Custom item delegate to draw a blue ratio bar inside '% of Parent' cells
class PercentDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit PercentDelegate(QObject* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
