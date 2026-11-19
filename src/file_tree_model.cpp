// ==============================================================================
// Program: file_tree_model.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Qt model interface mapping tree node listings and dynamic column sizing
//
// How it fits: MainWindow owns one FileTreeModel and attaches it to a
// QTreeView. The Scanner owns the TreeNode tree; this class only adapts it to
// Qt's model/view interface (it never owns nodes) and PercentDelegate draws the
// "% of Parent" bar. Columns: 0 Name, 1 % of Parent, 2 Size, 3 Allocated,
// 4 Files, 5 Folders (see headerData and data).
//
// Key design decision: every QModelIndex stores the TreeNode* as its internal
// pointer, so no per-row wrapper objects exist (a scan can produce millions of
// nodes). The cost is that nodes must outlive every index handed to a view;
// anything that frees nodes must go through removeNode() or setRootNode().
//
// Locking: index/parent/rowCount take the node's shared lock because scan
// threads may still be inserting while the GUI repaints. sort() does not lock;
// see the warning there.
// ==============================================================================
#include "file_tree_model.hpp"
#include "theme_manager.hpp"
#include <QApplication>
#include <QStyle>
#include <QIcon>
#include <QPainter>
#include <mutex>
#include <algorithm>

/**
 * @param root   Tree to display (not owned); may be nullptr for an empty view.
 * @param parent Qt parent object.
 */
FileTreeModel::FileTreeModel(TreeNode* root, QObject* parent)
    : QAbstractItemModel(parent), m_root(root) {}

/**
 * Replaces the displayed tree (or clears it with nullptr).
 * Wrapping the swap in beginResetModel/endResetModel tells every attached view
 * to discard cached indexes, because each TreeNode pointer they hold may now
 * dangle.
 */
void FileTreeModel::setRootNode(TreeNode* root) {
    beginResetModel();
    m_root = root;
    endResetModel();
}

/**
 * Builds the index for child number @p row of @p parent.
 * An invalid @p parent means the (hidden) root. The node pointer is stored as
 * the index's internal pointer. The children vector is read under the parent's
 * shared lock because scanner threads may still be inserting nodes.
 * @return Invalid index if the row is out of range.
 */
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

/**
 * Returns the index of @p child's parent, as Qt requires for tree views.
 * Qt needs the parent's ROW inside the grandparent, found by a linear search of
 * the grandparent's children (cost grows with directory width). Children of the
 * root report an invalid parent: they are the top-level rows.
 */
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

/**
 * Number of children of @p parent. Files have none; directories report their
 * current child count (under a shared lock). Called very often by the view.
 */
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

/** Always 6; see the column list in the file header. */
int FileTreeModel::columnCount(const QModelIndex& /*parent*/) const {
    return 6; // Name, % of Parent, Size, Allocated, Files, Folders
}

/**
 * Supplies cell contents and appearance.
 *
 * Qt::DisplayRole gives the text; on column 1 Qt::UserRole gives the raw
 * percentage (a double) that PercentDelegate uses to size its bar. Percent is
 * the node's size relative to its parent's size (100 when the parent is empty).
 * Files show no Files/Folders counts. Icons come from the application style
 * (no image assets). Numeric columns are right-aligned.
 */
QVariant FileTreeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return QVariant();

    TreeNode* node = static_cast<TreeNode*>(index.internalPointer());
    if (!node) return QVariant();

    if (role == Qt::DisplayRole || role == Qt::UserRole) {
        if (index.column() == 1) {
            // Percent of the parent's size; a zero-size parent would divide by zero, so
            // the default 100 is kept in that case.
            double pct = 100.0;
            if (node->parent && node->parent->size > 0) {
                pct = (static_cast<double>(node->size) / node->parent->size) * 100.0;
            // Defensive branch for a node without a parent (only reachable for the root).
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
    // Standard icons keep the program free of external image files.
    } else if (role == Qt::DecorationRole && index.column() == 0) {
        // Return standard icons to avoid large external assets
        if (node->is_dir) {
            return QApplication::style()->standardIcon(QStyle::SP_DirIcon);
        } else {
            return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
        }
    // Everything except the name is a number, so right-align it for easy comparison.
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() > 0) {
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
    }

    return QVariant();
}

/** Column titles (translated with tr()). */
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

/**
 * Formats a byte count for display, e.g. 1536 -> "1.50 KB".
 * Uses binary units (1 KB = 1024 B) up to TB; plain bytes show no decimals,
 * everything else two.
 */
QString FileTreeModel::formatSize(uint64_t bytes) {
    double size = bytes;
    QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    // Divide until the number fits the unit; stop at the largest unit (TB).
    while (size >= 1024 && unitIndex < units.size() - 1) {
        size /= 1024;
        unitIndex++;
    }
    return QString::number(size, 'f', unitIndex == 0 ? 0 : 2) + " " + units[unitIndex];
}

/**
 * Sorts every level of the tree by @p column and resets the model.
 *
 * Column meanings: 0 name, 1 and 2 size (percent is proportional to size),
 * 3 allocated size, 4 file count, 5 folder count. Ties fall back to name so
 * the order is stable.
 *
 * WARNING: sorts in place WITHOUT taking node locks, so it must not run while a
 * scan is writing to the tree (MainWindow only enables sorting after the scan
 * ends). It also leaves `children` in non-name order, so
 * TreeNode::find_child / get_or_create_child must not be used on the sorted
 * tree afterwards; a new scan builds a fresh tree, so this is not hit today.
 */
void FileTreeModel::sort(int column, Qt::SortOrder order) {
    if (!m_root) return;

    // Reset (rather than layoutChanged) because the node pointers inside existing
    // persistent indexes would now refer to different rows.
    beginResetModel();

    // Recursive generic lambda: passes itself as `self` so it can recurse without std::function overhead.
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
        // Sort descendants too so every expanded folder is ordered the same way.
        for (auto& child : node->children) {
            self(self, child.get(), col, ord);
        }
    };

    recursive_sort(recursive_sort, m_root, column, order);

    endResetModel();
}

/**
 * Removes one node (and its whole subtree) from the model.
 * Does NOT adjust ancestor totals; MainWindow does that after the item has been
 * moved to the trash. Brackets the erase with beginRemoveRows/endRemoveRows so
 * attached views update correctly.
 * @return false if the index is invalid, is the root or is out of range.
 */
bool FileTreeModel::removeNode(const QModelIndex& index) {
    if (!index.isValid()) return false;
    TreeNode* node = static_cast<TreeNode*>(index.internalPointer());
    if (!node) return false;
    TreeNode* parentNode = node->parent;
    if (!parentNode) return false;

    // Range check under a shared lock, released before taking the exclusive lock
    // for the erase (a lock cannot be upgraded in place).
    int row = index.row();
    std::shared_lock<std::shared_mutex> size_lock(parentNode->mutex);
    if (row < 0 || row >= static_cast<int>(parentNode->children.size())) return false;
    size_lock.unlock();

    // Qt must be told BEFORE the rows disappear so views can drop selections.
    beginRemoveRows(index.parent(), row, row);
    {
        std::unique_lock<std::shared_mutex> lock(parentNode->mutex);
        parentNode->children.erase(parentNode->children.begin() + row);
    }
    endRemoveRows();
    return true;
}

/** Delegate that paints the "% of Parent" cell as a bar plus text. */
PercentDelegate::PercentDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

/**
 * Paints one cell: the normal item background without text, then a bar
 * proportional to the percentage (clamped to 0-100, 2 px inset) in the theme's
 * bar colour, then the percentage text centred on top (highlight colour when
 * the row is selected so it stays readable).
 */
void PercentDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QStyleOptionViewItem opt = option;
    // Clearing the text lets the base style draw background/selection only; the
    // text is drawn manually after the bar so the bar does not cover it.
    initStyleOption(&opt, index);
    opt.text.clear();

    if (opt.widget) {
        opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }

    // Raw percentage supplied by FileTreeModel::data() for Qt::UserRole.
    double pct = index.data(Qt::UserRole).toDouble();
    if (pct < 0.0) pct = 0.0;
    if (pct > 100.0) pct = 100.0;

    QRect rect = option.rect;
    int margin = 2;
    QRect barRect = rect.adjusted(margin, margin, -margin, -margin);
    // Width = percentage of the inner width; integer pixels, never negative.
    int barWidth = static_cast<int>(barRect.width() * (pct / 100.0));
    barRect.setWidth(std::max(0, barWidth));

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(Qt::NoPen);
    painter->setBrush(ThemeManager::instance().activeTheme().percentBarColor);
    painter->drawRect(barRect);
    painter->restore();

    // Text colour at alpha 224 of 255: slightly softened compared with plain text.
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
