#pragma once
#include <QAbstractItemModel>
#include <QStyledItemDelegate>
#include "tree_node.hpp"

class FileTreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit FileTreeModel(TreeNode* root = nullptr, QObject* parent = nullptr);
    ~FileTreeModel() override = default;

    void setRootNode(TreeNode* root);

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    bool removeNode(const QModelIndex& index);

    // Helper to format byte sizes
    static QString formatSize(uint64_t bytes);


private:
    TreeNode* m_root;
};

// Custom item delegate to draw a blue ratio bar inside '% of Parent' cells
class PercentDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit PercentDelegate(QObject* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
