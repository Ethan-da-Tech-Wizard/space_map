#include "file_tree_model.hpp"
#include <QApplication>
#include <QStyle>
#include <QIcon>
#include <QPainter>
#include <mutex>
#include <algorithm>

FileTreeModel::FileTreeModel(TreeNode* root, QObject* parent)
    : QAbstractItemModel(parent), m_root(root) {}

void FileTreeModel::setRootNode(TreeNode* root) {
    beginResetModel();
    m_root = root;
    endResetModel();
}

QModelIndex FileTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!m_root) return QModelIndex();

    TreeNode* parentNode = nullptr;
    if (!parent.isValid()) {
        parentNode = m_root;
    } else {
        parentNode = static_cast<TreeNode*>(parent.internalPointer());
    }

    if (!parentNode) return QModelIndex();

    std::shared_lock<std::shared_mutex> lock(parentNode->mutex);
    if (row >= 0 && row < static_cast<int>(parentNode->children.size())) {
        return createIndex(row, column, parentNode->children[row].get());
    }
    return QModelIndex();
}

QModelIndex FileTreeModel::parent(const QModelIndex& child) const {
    if (!child.isValid()) return QModelIndex();

    TreeNode* childNode = static_cast<TreeNode*>(child.internalPointer());
    if (!childNode) return QModelIndex();

    TreeNode* parentNode = childNode->parent;
    if (!parentNode || parentNode == m_root) {
        return QModelIndex();
    }

    TreeNode* grandParentNode = parentNode->parent;
    if (!grandParentNode) return QModelIndex();

    std::shared_lock<std::shared_mutex> lock(grandParentNode->mutex);
    auto it = std::find_if(grandParentNode->children.begin(), grandParentNode->children.end(),
        [parentNode](const std::unique_ptr<TreeNode>& ptr) {
            return ptr.get() == parentNode;
        });

    if (it != grandParentNode->children.end()) {
        int row = static_cast<int>(std::distance(grandParentNode->children.begin(), it));
        return createIndex(row, 0, parentNode);
    }
    return QModelIndex();
}

int FileTreeModel::rowCount(const QModelIndex& parent) const {
    if (!m_root) return 0;

    TreeNode* parentNode = nullptr;
    if (!parent.isValid()) {
        parentNode = m_root;
    } else {
        parentNode = static_cast<TreeNode*>(parent.internalPointer());
    }

    if (!parentNode || !parentNode->is_dir) return 0;

    std::shared_lock<std::shared_mutex> lock(parentNode->mutex);
    return static_cast<int>(parentNode->children.size());
}

int FileTreeModel::columnCount(const QModelIndex& /*parent*/) const {
    return 6; // Name, % of Parent, Size, Allocated, Files, Folders
}

QVariant FileTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();

    TreeNode* node = static_cast<TreeNode*>(index.internalPointer());
    if (!node) return QVariant();

    if (role == Qt::DisplayRole || role == Qt::UserRole) {
        if (index.column() == 1) {
            double pct = 100.0;
            if (node->parent && node->parent->size > 0) {
                pct = (static_cast<double>(node->size) / node->parent->size) * 100.0;
            } else if (!node->parent && m_root && m_root->size > 0) {
                // If it is the top level under root
                pct = (static_cast<double>(node->size) / m_root->size) * 100.0;
            }
            if (role == Qt::UserRole) {
                return pct;
            }
            return QString::number(pct, 'f', 1) + " %";
        }
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0:
                return QString::fromStdString(node->name);
            case 2:
                return formatSize(node->size);
            case 3:
                return formatSize(node->allocated_size);
            case 4:
                return node->is_dir ? QVariant(static_cast<qlonglong>(node->file_count)) : QVariant();
            case 5:
                return node->is_dir ? QVariant(static_cast<qlonglong>(node->dir_count)) : QVariant();
            default:
                break;
        }
    } else if (role == Qt::DecorationRole && index.column() == 0) {
        // Return standard icons to avoid large external assets
        if (node->is_dir) {
            return QApplication::style()->standardIcon(QStyle::SP_DirIcon);
        } else {
            return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() > 0) {
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    }

    return QVariant();
}

QVariant FileTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return tr("Name");
            case 1: return tr("% of Parent");
            case 2: return tr("Size");
            case 3: return tr("Allocated");
            case 4: return tr("Files");
            case 5: return tr("Folders");
            default: break;
        }
    }
    return QVariant();
}

QString FileTreeModel::formatSize(uint64_t bytes) {
    double size = bytes;
    QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    while (size >= 1024 && unitIndex < units.size() - 1) {
        size /= 1024;
        unitIndex++;
    }
    return QString::number(size, 'f', unitIndex == 0 ? 0 : 2) + " " + units[unitIndex];
}

void FileTreeModel::sort(int column, Qt::SortOrder order) {
    if (!m_root) return;

    beginResetModel();

    auto recursive_sort = [](auto& self, TreeNode* node, int col, Qt::SortOrder ord) -> void {
        std::sort(node->children.begin(), node->children.end(),
            [col, ord](const std::unique_ptr<TreeNode>& a, const std::unique_ptr<TreeNode>& b) {
                switch (col) {
                    case 0: // Name
                        if (ord == Qt::AscendingOrder) {
                            return a->name < b->name;
                        } else {
                            return a->name > b->name;
                        }
                    case 1: // % of Parent (same as size)
                    case 2: // Size
                        if (a->size != b->size) {
                            if (ord == Qt::AscendingOrder) {
                                return a->size < b->size;
                            } else {
                                return a->size > b->size;
                            }
                        }
                        return a->name < b->name; // Alphabetical fallback
                    case 3: // Allocated
                        if (a->allocated_size != b->allocated_size) {
                            if (ord == Qt::AscendingOrder) {
                                return a->allocated_size < b->allocated_size;
                            } else {
                                return a->allocated_size > b->allocated_size;
                            }
                        }
                        return a->name < b->name; // Alphabetical fallback
                    case 4: // Files
                        if (a->file_count != b->file_count) {
                            if (ord == Qt::AscendingOrder) {
                                return a->file_count < b->file_count;
                            } else {
                                return a->file_count > b->file_count;
                            }
                        }
                        return a->name < b->name; // Alphabetical fallback
                    case 5: // Folders
                        if (a->dir_count != b->dir_count) {
                            if (ord == Qt::AscendingOrder) {
                                return a->dir_count < b->dir_count;
                            } else {
                                return a->dir_count > b->dir_count;
                            }
                        }
                        return a->name < b->name; // Alphabetical fallback
                    default:
                        return false;
                }
            });
        for (auto& child : node->children) {
            self(self, child.get(), col, ord);
        }
    };

    recursive_sort(recursive_sort, m_root, column, order);

    endResetModel();
}

bool FileTreeModel::removeNode(const QModelIndex& index) {
    if (!index.isValid()) return false;
    TreeNode* node = static_cast<TreeNode*>(index.internalPointer());
    if (!node) return false;
    TreeNode* parentNode = node->parent;
    if (!parentNode) return false;

    int row = index.row();
    std::shared_lock<std::shared_mutex> size_lock(parentNode->mutex);
    if (row < 0 || row >= static_cast<int>(parentNode->children.size())) return false;
    size_lock.unlock();

    beginRemoveRows(index.parent(), row, row);
    {
        std::unique_lock<std::shared_mutex> lock(parentNode->mutex);
        parentNode->children.erase(parentNode->children.begin() + row);
    }
    endRemoveRows();
    return true;
}

PercentDelegate::PercentDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void PercentDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text.clear();

    if (opt.widget) {
        opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }

    double pct = index.data(Qt::UserRole).toDouble();
    if (pct < 0.0) pct = 0.0;
    if (pct > 100.0) pct = 100.0;

    QRect rect = option.rect;
    int margin = 2;
    QRect barRect = rect.adjusted(margin, margin, -margin, -margin);
    int barWidth = static_cast<int>(barRect.width() * (pct / 100.0));
    barRect.setWidth(std::max(0, barWidth));

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(70, 130, 180, 120)); // Semi-transparent steel-blue
    painter->drawRect(barRect);
    painter->restore();

    QString text = index.data(Qt::DisplayRole).toString();
    painter->save();
    QColor textColor = QColor(224, 224, 224);
    if (option.state & QStyle::State_Selected) {
        textColor = option.palette.color(QPalette::HighlightedText);
    }
    painter->setPen(textColor);
    painter->setFont(option.font);
    painter->drawText(rect, Qt::AlignCenter, text);
    painter->restore();
}
