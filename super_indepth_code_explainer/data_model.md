# SpaceMap Architecture Book: Chapter 6

This chapter provides a complete line-by-line, character-by-character architectural explanation of the Qt Data Model and Percent Delegate (`src/file_tree_model.hpp` and `src/file_tree_model.cpp`).

---

# CHAPTER 6: Model-View-Controller & Custom Canvas Drawing

Qt separates layouts from datasets using a variation of the Model-View-Controller pattern. The **View** (`QTreeView`) manages drawing and input events, and has no reference to memory nodes. The **Model** (`FileTreeModel`) acts as the translator, mapping our custom `TreeNode` node addresses to standard `QModelIndex` coordinate coordinates. This chapter explains this bridge and how custom percentage progress bars are rendered within table grid cells.

---

## 1. src/file_tree_model.hpp (Header File Breakdown)

The header defines the model's interface overrides and the custom delegate class.

```cpp
1: #pragma once
```
* **Explanation**: Header guard.

```cpp
2: #include <QAbstractItemModel>
3: #include <QStyledItemDelegate>
4: #include "tree_node.hpp"
```
* **Explanation**:
  * `<QAbstractItemModel>`: Exposes the base class required to construct custom hierarchical models.
  * `<QStyledItemDelegate>`: Exposes the standard base delegate for custom rendering.
  * `"tree_node.hpp"`: Core TreeNode structure.

```cpp
6: class FileTreeModel : public QAbstractItemModel {
7:     Q_OBJECT
```
* **Explanation**: Inherits from `QAbstractItemModel` and registers the `Q_OBJECT` macro to enable Qt signal/slot features.

```cpp
8: public:
9:     explicit FileTreeModel(TreeNode* root = nullptr, QObject* parent = nullptr);
10:     ~FileTreeModel() override = default;
```
* **Explanation**: Constructor and virtual destructor.

```cpp
12:     void setRootNode(TreeNode* root);
```
* **Explanation**: Assigns a new scanning root node to the model, refreshing the display table.

```cpp
15:     QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
```
* **Explanation**: Generates a coordinate token `QModelIndex` mapping a child row/column to its parent.

```cpp
16:     QModelIndex parent(const QModelIndex& child) const override;
```
* **Explanation**: Returns the parent index token of a given child index.

```cpp
17:     int rowCount(const QModelIndex& parent = QModelIndex()) const override;
```
* **Explanation**: Returns the child count for a given parent index.

```cpp
18:     int columnCount(const QModelIndex& parent = QModelIndex()) const override;
```
* **Explanation**: Returns total grid columns (returns 6).

```cpp
19:     QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
```
* **Explanation**: Returns cell values (strings, icons, alignments, raw ratios) based on requested role queries.

```cpp
20:     QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
```
* **Explanation**: Returns table column header text.

```cpp
21:     void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;
```
* **Explanation**: Performs recursive sorting on tree branches.

```cpp
23:     bool removeNode(const QModelIndex& index);
```
* **Explanation**: Safely deletes a node row from the model.

```cpp
26:     static QString formatSize(uint64_t bytes);
```
* **Explanation**: Converts byte counts to readable units (KB, MB, GB, TB).

```cpp
29: private:
30:     TreeNode* m_root;
31: };
```
* **Explanation**: Raw pointer to the root `TreeNode` in memory (not owned by the model).

```cpp
34: class PercentDelegate : public QStyledItemDelegate {
35:     Q_OBJECT
36: public:
37:     explicit PercentDelegate(QObject* parent = nullptr);
38:     void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
39: };
```
* **Explanation**: Custom delegate overriding `paint()` to draw ratio progress bars in Column 1.

---

## 2. src/file_tree_model.cpp (Source File Breakdown)

The source file handles coordinate translation, recursive sorting, and custom painting.

```cpp
9: FileTreeModel::FileTreeModel(TreeNode* root, QObject* parent)
10:     : QAbstractItemModel(parent), m_root(root) {}
```
* **Explanation**: Constructor. Passes parent pointer to base class and stores root pointer.

```cpp
12: void FileTreeModel::setRootNode(TreeNode* root) {
13:     beginResetModel();
14:     m_root = root;
15:     endResetModel();
16: }
```
* **Explanation**:
  * `beginResetModel()` / `endResetModel()`: Notifies the tree view to discard cache, selections, and layouts, and redraw everything using the new root pointer.

```cpp
18: QModelIndex FileTreeModel::index(int row, int column, const QModelIndex& parent) const {
19:     if (!m_root) return QModelIndex();
```
* **Explanation**: Returns index. Aborts if no root node exists.

```cpp
21:     TreeNode* parentNode = nullptr;
22:     if (!parent.isValid()) {
23:         parentNode = m_root;
24:     } else {
25:         parentNode = static_cast<TreeNode*>(parent.internalPointer());
26:     }
```
* **Explanation**: Resolves the parent node. If the parent coordinate is invalid, defaults to root node. Otherwise, extracts the raw parent address via `internalPointer()`.

```cpp
28:     if (!parentNode) return QModelIndex();
29: 
30:     std::shared_lock<std::shared_mutex> lock(parentNode->mutex);
31:     if (row >= 0 && row < static_cast<int>(parentNode->children.size())) {
32:         return createIndex(row, column, parentNode->children[row].get());
33:     }
34:     return QModelIndex();
35: }
```
* **Explanation**:
  * Locks parent children structure for reading.
  * `createIndex(row, column, pointer)`: Packages the coordinate offsets alongside the raw child pointer, allowing instant lookups during future queries.

```cpp
37: QModelIndex FileTreeModel::parent(const QModelIndex& child) const {
38:     if (!child.isValid()) return QModelIndex();
39: 
40:     TreeNode* childNode = static_cast<TreeNode*>(child.internalPointer());
41:     if (!childNode) return QModelIndex();
```
* **Explanation**: Resolves child parent index. Returns empty if invalid.

```cpp
43:     TreeNode* parentNode = childNode->parent;
44:     if (!parentNode || parentNode == m_root) {
45:         return QModelIndex();
46:     }
```
* **Explanation**: If child parent is null or is root, returns empty index (no grandparent exists).

```cpp
48:     TreeNode* grandParentNode = parentNode->parent;
49:     if (!grandParentNode) return QModelIndex();
```
* **Explanation**: Resolves grandparent node.

```cpp
51:     std::shared_lock<std::shared_mutex> lock(grandParentNode->mutex);
52:     auto it = std::find_if(grandParentNode->children.begin(), grandParentNode->children.end(),
53:         [parentNode](const std::unique_ptr<TreeNode>& ptr) {
54:             return ptr.get() == parentNode;
55:         });
```
* **Explanation**:
  * Locks grandparent children lists.
  * `std::find_if`: Linear search to find parent node's index position inside grandparent's children vector.

```cpp
57:     if (it != grandParentNode->children.end()) {
58:         int row = static_cast<int>(std::distance(grandParentNode->children.begin(), it));
59:         return createIndex(row, 0, parentNode);
60:     }
61:     return QModelIndex();
62: }
```
* **Explanation**: Calculates row offset using `std::distance` and packages parent index mapping it to column 0.

```cpp
64: int FileTreeModel::rowCount(const QModelIndex& parent) const {
65:     if (!m_root) return 0;
66: 
67:     TreeNode* parentNode = nullptr;
68:     if (!parent.isValid()) {
69:         parentNode = m_root;
70:     } else {
71:         parentNode = static_cast<TreeNode*>(parent.internalPointer());
72:     }
```
* **Explanation**: Resolves node pointer.

```cpp
74:     if (!parentNode || !parentNode->is_dir) return 0;
75: 
76:     std::shared_lock<std::shared_mutex> lock(parentNode->mutex);
77:     return static_cast<int>(parentNode->children.size());
78: }
```
* **Explanation**: Returns size of children array under read lock if parent is directory. Files return 0.

```cpp
80: int FileTreeModel::columnCount(const QModelIndex& /*parent*/) const {
81:     return 6; // Name, % of Parent, Size, Allocated, Files, Folders
82: }
```
* **Explanation**: Columns count constant.

```cpp
84: QVariant FileTreeModel::data(const QModelIndex& index, int role) const {
85:     if (!index.isValid()) return QVariant();
86: 
87:     TreeNode* node = static_cast<TreeNode*>(index.internalPointer());
88:     if (!node) return QVariant();
```
* **Explanation**: Extracts node target pointer.

```cpp
90:     if (role == Qt::DisplayRole || role == Qt::UserRole) {
91:         if (index.column() == 1) {
92:             double pct = 100.0;
93:             if (node->parent && node->parent->size > 0) {
94:                 pct = (static_cast<double>(node->size) / node->parent->size) * 100.0;
95:             } else if (!node->parent && m_root && m_root->size > 0) {
96:                 // If it is the top level under root
97:                 pct = (static_cast<double>(node->size) / m_root->size) * 100.0;
98:             }
```
* **Explanation**: Computes percentage relative to parent size (or root size for top level items).

```cpp
99:             if (role == Qt::UserRole) {
100:                 return pct;
101:             }
102:             return QString::number(pct, 'f', 1) + " %";
103:         }
104:     }
```
* **Explanation**:
  * `Qt::UserRole`: Returns raw `double` ratio values for the custom bar delegate calculations.
  * `Qt::DisplayRole`: Returns formatted percentage strings (e.g. `12.5 %`).

```cpp
106:     if (role == Qt::DisplayRole) {
107:         switch (index.column()) {
108:             case 0:
109:                 return QString::fromStdString(node->name);
110:             case 2:
111:                 return formatSize(node->size);
112:             case 3:
113:                 return formatSize(node->allocated_size);
114:             case 4:
115:                 return node->is_dir ? QVariant(static_cast<qlonglong>(node->file_count)) : QVariant();
116:             case 5:
117:                 return node->is_dir ? QVariant(static_cast<qlonglong>(node->dir_count)) : QVariant();
118:             default:
119:                 break;
120:         }
```
* **Explanation**: Maps other columns to their respective data fields. Only displays file/folder counts for directories.

```cpp
121:     } else if (role == Qt::DecorationRole && index.column() == 0) {
122:         // Return standard icons to avoid large external assets
123:         if (node->is_dir) {
124:             return QApplication::style()->standardIcon(QStyle::SP_DirIcon);
125:         } else {
126:             return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
127:         }
```
* **Explanation**:
  * `Qt::DecorationRole`: Returns standard system icons (directory vs file icon) for Column 0.

```cpp
128:     } else if (role == Qt::TextAlignmentRole) {
129:         if (index.column() > 0) {
130:             return QVariant(Qt::AlignRight | Qt::AlignVCenter);
131:         }
132:         return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
133:     }
134: 
135:     return QVariant();
136: }
```
* **Explanation**: Configures text alignments (right-aligned for numerical data, left-aligned for names).

```cpp
138: QVariant FileTreeModel::headerData(int section, Qt::Orientation orientation, int role) const {
139:     if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
140:         switch (section) {
141:             case 0: return tr("Name");
142:             case 1: return tr("% of Parent");
143:             case 2: return tr("Size");
144:             case 3: return tr("Allocated");
145:             case 4: return tr("Files");
146:             case 5: return tr("Folders");
147:             default: break;
148:         }
149:     }
150:     return QVariant();
151: }
```
* **Explanation**: Returns horizontal header text label mappings.

```cpp
153: QString FileTreeModel::formatSize(uint64_t bytes) {
154:     double size = bytes;
155:     QStringList units = {"B", "KB", "MB", "GB", "TB"};
156:     int unitIndex = 0;
157:     while (size >= 1024 && unitIndex < units.size() - 1) {
158:         size /= 1024;
159:         unitIndex++;
160:     }
161:     return QString::number(size, 'f', unitIndex == 0 ? 0 : 2) + " " + units[unitIndex];
162: }
```
* **Explanation**: Converts byte counts to readable units, adding two decimal places for units larger than Bytes.

```cpp
164: void FileTreeModel::sort(int column, Qt::SortOrder order) {
165:     if (!m_root) return;
166: 
167:     beginResetModel();
```
* **Explanation**: Triggers view updates during sorting.

```cpp
169:     auto recursive_sort = [](auto& self, TreeNode* node, int col, Qt::SortOrder ord) -> void {
170:         std::sort(node->children.begin(), node->children.end(),
171:             [col, ord](const std::unique_ptr<TreeNode>& a, const std::unique_ptr<TreeNode>& b) {
```
* **Explanation**: Recursively sorts node children using comparator lambda functions based on columns and order.

```cpp
172:                 switch (col) {
173:                     case 0: // Name
174:                         if (ord == Qt::AscendingOrder) {
175:                             return a->name < b->name;
176:                         } else {
177:                             return a->name > b->name;
178:                         }
179:                     case 1: // % of Parent (same as size)
180:                     case 2: // Size
181:                         if (a->size != b->size) {
182:                             if (ord == Qt::AscendingOrder) {
183:                                 return a->size < b->size;
184:                             } else {
185:                                 return a->size > b->size;
186:                             }
187:                         }
188:                         return a->name < b->name; // Alphabetical fallback
```
* **Explanation**: Compares node sizes, falling back to alphabetical sorting by name if equal. This satisfies the strict weak ordering requirement.

```cpp
189:                     case 3: // Allocated
190:                         if (a->allocated_size != b->allocated_size) {
191:                             if (ord == Qt::AscendingOrder) {
192:                                 return a->allocated_size < b->allocated_size;
193:                             } else {
194:                                 return a->allocated_size > b->allocated_size;
195:                             }
196:                         }
197:                         return a->name < b->name;
198:                     case 4: // Files
199:                         if (a->file_count != b->file_count) {
200:                             if (ord == Qt::AscendingOrder) {
201:                                 return a->file_count < b->file_count;
202:                             } else {
203:                                 return a->file_count > b->file_count;
204:                             }
205:                         }
206:                         return a->name < b->name;
207:                     case 5: // Folders
208:                         if (a->dir_count != b->dir_count) {
209:                             if (ord == Qt::AscendingOrder) {
210:                                 return a->dir_count < b->dir_count;
211:                             } else {
212:                                 return a->dir_count > b->dir_count;
213:                             }
214:                         }
215:                         return a->name < b->name;
216:                     default:
217:                         return false;
218:                 }
219:             });
```
* **Explanation**: Handles sorting comparators for remaining columns with alphabetical fallbacks.

```cpp
220:         for (auto& child : node->children) {
221:             self(self, child.get(), col, ord);
222:         }
223:     };
224: 
225:     recursive_sort(recursive_sort, m_root, column, order);
226: 
227:     endResetModel();
228: }
```
* **Explanation**: Recursively sorts children and notifies model reset is complete.

```cpp
230: bool FileTreeModel::removeNode(const QModelIndex& index) {
231:     if (!index.isValid()) return false;
232:     TreeNode* node = static_cast<TreeNode*>(index.internalPointer());
233:     if (!node) return false;
234:     TreeNode* parentNode = node->parent;
235:     if (!parentNode) return false;
```
* **Explanation**: Resolves node to remove. Aborts if node is root or parent is null.

```cpp
237:     int row = index.row();
238:     std::shared_lock<std::shared_mutex> size_lock(parentNode->mutex);
239:     if (row < 0 || row >= static_cast<int>(parentNode->children.size())) return false;
240:     size_lock.unlock();
```
* **Explanation**: Verifies the target row index is within parent children vector limits under a read lock.

```cpp
242:     beginRemoveRows(index.parent(), row, row);
```
* **Explanation**: Notifies view that the row is about to be deleted, allowing it to update index caching.

```cpp
243:     {
244:         std::unique_lock<std::shared_mutex> lock(parentNode->mutex);
245:         parentNode->children.erase(parentNode->children.begin() + row);
246:     }
```
* **Explanation**:
  * Locks parent exclusively.
  * Erases the child pointer, which deletes the child node's memory.

```cpp
247:     endRemoveRows();
248:     return true;
249: }
```
* **Explanation**: Notifies model that row removal is complete.

```cpp
251: PercentDelegate::PercentDelegate(QObject* parent)
252:     : QStyledItemDelegate(parent) {}
```
* **Explanation**: Delegate constructor.

```cpp
254: void PercentDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
255:     QStyleOptionViewItem opt = option;
256:     initStyleOption(&opt, index);
257:     opt.text.clear();
```
* **Explanation**:
  * Custom Painter Override:
  * `opt.text.clear()`: Clears the display text from the options structure so the default item delegate doesn't paint it. This allows us to draw the text manually on top of the custom ratio progress bar.

```cpp
259:     if (opt.widget) {
260:         opt.widget->style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
261:     } else {
262:         QStyledItemDelegate::paint(painter, option, index);
263:     }
```
* **Explanation**: Renders standard selection highlights and borders.

```cpp
265:     double pct = index.data(Qt::UserRole).toDouble();
266:     if (pct < 0.0) pct = 0.0;
267:     if (pct > 100.0) pct = 100.0;
```
* **Explanation**: Queries the raw double ratio percentage value via `Qt::UserRole` and caps it between 0.0 and 100.0.

```cpp
269:     QRect rect = option.rect;
270:     int margin = 2;
271:     QRect barRect = rect.adjusted(margin, margin, -margin, -margin);
272:     int barWidth = static_cast<int>(barRect.width() * (pct / 100.0));
273:     barRect.setWidth(std::max(0, barWidth));
```
* **Explanation**:
  * Adjusts the cell bounding box `rect` to insert a 2-pixel margin.
  * Computes the progress bar's pixel width based on the percentage value.

```cpp
275:     painter->save();
276:     painter->setRenderHint(QPainter::Antialiasing);
277:     painter->setPen(Qt::NoPen);
278:     painter->setBrush(QColor(70, 130, 180, 120)); // Semi-transparent steel-blue
279:     painter->drawRect(barRect);
280:     painter->restore();
```
* **Explanation**:
  * `painter->save()` / `painter->restore()`: Saves and restores the graphics state so we don't bleed our brush configurations into subsequent cell drawing cycles.
  * Renders a semi-transparent steel-blue (`#4682B4` with alpha 120) rectangle directly onto the cell canvas.

```cpp
282:     QString text = index.data(Qt::DisplayRole).toString();
283:     painter->save();
284:     QColor textColor = QColor(224, 224, 224);
285:     if (option.state & QStyle::State_Selected) {
286:         textColor = option.palette.color(QPalette::HighlightedText);
287:     }
288:     painter->setPen(textColor);
289:     painter->setFont(option.font);
290:     painter->drawText(rect, Qt::AlignCenter, text);
291:     painter->restore();
292: }
```
* **Explanation**:
  * Retrieves formatted percentage text.
  * If the cell is selected, uses the system's HighlightedText color, otherwise uses light grey (`#e0e0e0`).
  * `drawText(...)`: Draws the percentage string centered inside the cell box, over the steel-blue bar.
