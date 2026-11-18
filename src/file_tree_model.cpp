// ==============================================================================
// Program: space_map - file_tree_model.cpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Implements FileTreeModel (index, parent, rows, cell data, sorting, node removal) and the PercentDelegate bar painter.
// ==============================================================================
#include "file_tree_model.hpp"
#include "theme_manager.hpp"
#include <QApplication>
#include <QStyle>
#include <QIcon>
#include <QPainter>
#include <mutex>
#include <algorithm>

/// Stores the (possibly null) root node; the model does not own it.
FileTreeModel::FileTreeModel(TreeNode* root, QObject* parent)
    : QAbstractItemModel(parent), m_root(root) {}

/// Replaces the root node, wrapped in begin/endResetModel so views refresh.
void FileTreeModel::setRootNode(TreeNode* root) {
    beginResetModel();
    m_root = root;
    endResetModel();
}

/// Creates an index for child 'row' of the parent; the TreeNode pointer is stored as the index's internal pointer.
QModelIndex FileTreeModel::index(int row, int column, const QModelIndex& parent) const {
    if (!m_root) return QModelIndex();

    TreeNode* parentNode = nullptr;
    if (!parent.isValid()) {
        parentNode = m_root;
    } else {
        parentNode = static_cast<TreeNode*>(parent.internalPointer());
    }

    if (!parentNode) return QModelIndex();

    // Read lock: the scanner may still be inserting children on other threads
    std::shared_lock<std::shared_mutex> lock(parentNode->mutex);
    if (row >= 0 && row < static_cast<int>(parentNode->children.size())) {
        return createIndex(row, column, parentNode->children[row].get());
    }
    return QModelIndex();
}

/// Finds the parent index by locating the parent's row among the grandparent's children.
/// The root and its direct children map to an invalid (top-level) index.
QModelIndex FileTreeModel::parent(const QModelIndex& child) const {
    if (!child.isValid()) return QModelIndex();

    TreeNode* childNode = static_cast<TreeNode*>(child.internalPointer());
    if (!childNode) return QModelIndex();

    // Items whose parent is the root are top-level rows in the view
    TreeNode* parentNode = childNode->parent;
    if (!parentNode || parentNode == m_root) {
        return QModelIndex();
    }

    TreeNode* grandParentNode = parentNode->parent;
    if (!grandParentNode) return QModelIndex();

    std::shared_lock<std::shared_mutex> lock(grandParentNode->mutex);
    // Linear scan to learn which row the parent occupies under the grandparent
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

/// Returns the child count for directories; files always have zero rows.
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

/// Always six columns.
int FileTreeModel::columnCount(const QModelIndex& /*parent*/) const {
    return 6; // Name, % of Parent, Size, Allocated, Files, Folders
}

/// Returns the value for one cell and role.
/// Column 1 returns a percentage (as a number for UserRole, as text for DisplayRole).
QVariant FileTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();

    TreeNode* node = static_cast<TreeNode*>(index.internalPointer());
    if (!node) return QVariant();

    if (role == Qt::DisplayRole || role == Qt::UserRole) {
        if (index.column() == 1) {
            // Share of the parent's total size; stays 100 when the parent size is zero
            double pct = 100.0;
            if (node->parent && node->parent->size > 0) {
                pct = (static_cast<double>(node->size) / node->parent->size) * 100.0;
            } else if (!node->parent && m_root && m_root->size > 0) {
                // Defensive case: a node without a parent pointer is compared against the model root instead
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

/// Returns the translated header text for each column.
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

/// Formats bytes with 1024-based units (B to TB); whole numbers for bytes, two decimals otherwise.
QString FileTreeModel::formatSize(uint64_t bytes) {
    double size = bytes;
    QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    // Divide down until the value fits the largest suitable unit
    while (size >= 1024 && unitIndex < units.size() - 1) {
        size /= 1024;
        unitIndex++;
    }
    return QString::number(size, 'f', unitIndex == 0 ? 0 : 2) + " " + units[unitIndex];
}

/// Sorts every directory's children by the given column and order.
/// Equal values fall back to name order so the result is stable and predictable.
void FileTreeModel::sort(int column, Qt::SortOrder order) {
    if (!m_root) return;

    beginResetModel();

    // Generic lambda that calls itself through its 'self' parameter to recurse into subdirectories
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

/// Erases a node from its parent's children and tells the view about the removed row.
/// Returns false if the index is invalid, has no parent, or the row is out of range.
bool FileTreeModel::removeNode(const QModelIndex& index) {
    if (!index.isValid()) return false;
    TreeNode* node = static_cast<TreeNode*>(index.internalPointer());
    if (!node) return false;
    TreeNode* parentNode = node->parent;
    if (!parentNode) return false;

    int row = index.row();
    // Bounds-check the row under a read lock, then release before the model signals
    std::shared_lock<std::shared_mutex> size_lock(parentNode->mutex);
    if (row < 0 || row >= static_cast<int>(parentNode->children.size())) return false;
    size_lock.unlock();

    // Announce the removal first, then erase under a write lock
    beginRemoveRows(index.parent(), row, row);
    {
        std::unique_lock<std::shared_mutex> lock(parentNode->mutex);
        parentNode->children.erase(parentNode->children.begin() + row);
    }
    endRemoveRows();
    return true;
}

/// Constructs the delegate with the usual item-delegate parent.
PercentDelegate::PercentDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

/// Paints the cell background, then a bar sized to the percentage, then the centered percent text.
void PercentDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    // Draw the normal cell (selection, icons) without text; the text is drawn on top of the bar later
    opt.text.clear();

    if (opt.widget) {
        opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }

    // Clamp to the 0-100 range so the bar never overflows the cell
    double pct = index.data(Qt::UserRole).toDouble();
    if (pct < 0.0) pct = 0.0;
    if (pct > 100.0) pct = 100.0;

    QRect rect = option.rect;
    // Inset the bar slightly and scale its width by the percentage
    int margin = 2;
    QRect barRect = rect.adjusted(margin, margin, -margin, -margin);
    int barWidth = static_cast<int>(barRect.width() * (pct / 100.0));
    barRect.setWidth(std::max(0, barWidth));

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(ThemeManager::instance().activeTheme().percentBarColor);
    painter->drawRect(barRect);
    painter->restore();

    // Draw the percent text over the bar, using the selection color when the row is selected
    QString text = index.data(Qt::DisplayRole).toString();
    painter->save();
    QColor textColor = ThemeManager::instance().activeTheme().textPrimary;
    textColor.setAlpha(224);
    if (option.state & QStyle::State_Selected) {
        textColor = option.palette.color(QPalette::HighlightedText);
    }
    painter->setPen(textColor);
    painter->setFont(option.font);
    painter->drawText(rect, Qt::AlignCenter, text);
    painter->restore();
}
