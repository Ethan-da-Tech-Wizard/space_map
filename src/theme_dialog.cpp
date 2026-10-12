#include "theme_dialog.hpp"
#include <QHBoxLayout>
#include <QLabel>
#include <QColorDialog>
#include <QFrame>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollBar>

ThemeDialog::ThemeDialog(QWidget* parent) : QDialog(parent) {
    m_originalTheme = ThemeManager::instance().activeTheme();
    m_editingTheme = m_originalTheme;
    m_selectedPresetIndex = ThemeManager::instance().activePresetIndex();
    m_selectedIsUser = ThemeManager::instance().isActiveUserPreset();

    setupUi();
    applyDialogStyle();
    loadThemeIntoEditor(m_editingTheme);
}

void ThemeDialog::setupUi() {
    setWindowTitle(tr("Customize Layout"));
    resize(860, 580);
    setMinimumSize(550, 360);

    QHBoxLayout* mainLayout = new QHBoxLayout();
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // =================== LEFT SIDE: PRESET GRID ===================
    QVBoxLayout* leftLayout = new QVBoxLayout();
    QLabel* presetTitle = new QLabel(tr("Presets"), this);
    presetTitle->setObjectName("sectionTitle");
    leftLayout->addWidget(presetTitle);

    m_presetScroll = new QScrollArea(this);
    m_presetScroll->setWidgetResizable(true);
    m_presetScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_presetScroll->setMinimumWidth(220);

    m_presetContainer = new QWidget();
    m_presetGrid = new QGridLayout(m_presetContainer);
    m_presetGrid->setSpacing(6);
    m_presetGrid->setContentsMargins(4, 4, 4, 4);

    rebuildPresetGrid();

    m_presetScroll->setWidget(m_presetContainer);
    leftLayout->addWidget(m_presetScroll, 1);

    mainLayout->addLayout(leftLayout, 1);

    // =================== RIGHT SIDE: COLOR EDITOR ===================
    QVBoxLayout* rightLayout = new QVBoxLayout();
    QLabel* editorTitle = new QLabel(tr("Customize Colors"), this);
    editorTitle->setObjectName("sectionTitle");
    rightLayout->addWidget(editorTitle);

    m_helpBanner = new QLabel(tr("💡 Click or hover any ? icon or color label below to see what it modifies!"), this);
    m_helpBanner->setObjectName("helpBanner");
    m_helpBanner->setWordWrap(true);
    m_helpBanner->setMinimumHeight(38);
    rightLayout->addWidget(m_helpBanner);

    m_editorScroll = new QScrollArea(this);
    m_editorScroll->setWidgetResizable(true);
    m_editorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_editorScroll->setMinimumWidth(260);

    QWidget* editorWidget = new QWidget();
    QVBoxLayout* editorLayout = new QVBoxLayout(editorWidget);
    editorLayout->setSpacing(4);
    editorLayout->setContentsMargins(6, 6, 6, 6);

    // Window & Panels
    addColorSection(editorLayout, "Window & Panels");
    addColorRow(editorLayout, "Window Background", "windowBg");
    addColorRow(editorLayout, "Panel Background", "panelBg");
    addColorRow(editorLayout, "HUD Background", "hudBg");
    addColorRow(editorLayout, "HUD Border", "hudBorder");

    // Inputs
    addColorSection(editorLayout, "Inputs");
    addColorRow(editorLayout, "Input Background", "inputBg");
    addColorRow(editorLayout, "Input Border", "inputBorder");
    addColorRow(editorLayout, "Input Focus Border", "inputFocusBorder");

    // Buttons
    addColorSection(editorLayout, "Buttons");
    addColorRow(editorLayout, "Button Background", "buttonBg");
    addColorRow(editorLayout, "Button Hover", "buttonHoverBg");
    addColorRow(editorLayout, "Button Pressed", "buttonPressedBg");
    addColorRow(editorLayout, "Button Border", "buttonBorder");
    addColorRow(editorLayout, "Button Text", "buttonText");

    // Text
    addColorSection(editorLayout, "Text");
    addColorRow(editorLayout, "Primary Text", "textPrimary");
    addColorRow(editorLayout, "Secondary Text", "textSecondary");

    // Tree View
    addColorSection(editorLayout, "Tree View");
    addColorRow(editorLayout, "Tree Background", "treeBg");
    addColorRow(editorLayout, "Tree Alt Row", "treeAltBg");
    addColorRow(editorLayout, "Tree Item Hover", "treeItemHover");
    addColorRow(editorLayout, "Tree Item Selected", "treeItemSelected");
    addColorRow(editorLayout, "Selected Text", "treeItemSelectedText");
    addColorRow(editorLayout, "Percent Bar", "percentBarColor");

    // Headers
    addColorSection(editorLayout, "Headers");
    addColorRow(editorLayout, "Header Background", "headerBg");
    addColorRow(editorLayout, "Header Text", "headerText");
    addColorRow(editorLayout, "Header Border", "headerBorder");

    // Progress Bar
    addColorSection(editorLayout, "Progress Bar");
    addColorRow(editorLayout, "Progress Background", "progressBg");
    addColorRow(editorLayout, "Progress Fill", "progressChunk");
    addColorRow(editorLayout, "Progress Text", "progressText");
    addColorRow(editorLayout, "Progress Border", "progressBorder");

    // Menus
    addColorSection(editorLayout, "Menus");
    addColorRow(editorLayout, "Menu Background", "menuBg");
    addColorRow(editorLayout, "Menu Text", "menuText");
    addColorRow(editorLayout, "Menu Selected", "menuSelectedBg");
    addColorRow(editorLayout, "Menu Border", "menuBorder");

    // Delete Button
    addColorSection(editorLayout, "Delete Button");
    addColorRow(editorLayout, "Delete Background", "deleteEnabledBg");
    addColorRow(editorLayout, "Delete Border", "deleteEnabledBorder");

    editorLayout->addStretch(1);
    m_editorScroll->setWidget(editorWidget);
    rightLayout->addWidget(m_editorScroll, 1);

    mainLayout->addLayout(rightLayout, 1);

    // =================== BOTTOM BAR ===================
    QVBoxLayout* outerLayout = new QVBoxLayout();
    outerLayout->addLayout(mainLayout, 1);

    QHBoxLayout* bottomBar = new QHBoxLayout();
    bottomBar->setSpacing(8);
    bottomBar->setContentsMargins(8, 4, 8, 8);

    QPushButton* resetBtn = new QPushButton(tr("Reset"), this);
    QPushButton* closeBtn = new QPushButton(tr("Close"), this);

    resetBtn->setObjectName("dialogBtn");
    closeBtn->setObjectName("dialogBtn");

    bottomBar->addWidget(resetBtn);
    bottomBar->addStretch(1);
    bottomBar->addWidget(closeBtn);
    outerLayout->addLayout(bottomBar);

    // Re-set layout on dialog since we wrapped mainLayout
    delete layout();
    setLayout(outerLayout);

    connect(resetBtn, &QPushButton::clicked, this, &ThemeDialog::onReset);
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        ThemeManager::instance().saveSettings();
        accept();
    });
}

void ThemeDialog::applyDialogStyle() {
    setStyleSheet(R"(
        QDialog {
            background-color: #1a1a1a;
        }
        QLabel {
            color: #e0e0e0;
            font-size: 12px;
        }
        QLabel#sectionTitle {
            color: #ffffff;
            font-size: 15px;
            font-weight: bold;
            padding: 4px 0px;
        }
        QLabel#sectionHeader {
            color: #00d2ff;
            font-size: 12px;
            font-weight: bold;
            padding: 6px 0px 2px 0px;
        }
        QLabel#helpBanner {
            color: #00d2ff;
            background-color: #112233;
            border: 1px solid #0088cc;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
            font-weight: bold;
        }
        QPushButton#helpIconBtn {
            color: #00d2ff;
            background-color: #1a2a35;
            border: 1px solid #0088cc;
            border-radius: 9px;
            font-size: 11px;
            font-weight: bold;
            min-width: 18px;
            max-width: 18px;
            min-height: 18px;
            max-height: 18px;
            padding: 0px;
        }
        QPushButton#helpIconBtn:hover {
            color: #ffffff;
            background-color: #0088cc;
            border: 1px solid #00d2ff;
        }
        QToolTip {
            background-color: #1f2833;
            color: #ffffff;
            border: 1px solid #00d2ff;
            border-radius: 6px;
            padding: 6px 10px;
            font-size: 12px;
        }
        QScrollArea {
            background-color: #1e1e1e;
            border: 1px solid #2d2d2d;
            border-radius: 4px;
        }
        QScrollArea > QWidget > QWidget {
            background-color: #1e1e1e;
        }
        QLineEdit {
            background-color: #252525;
            color: #e0e0e0;
            border: 1px solid #3a3a3a;
            padding: 3px 6px;
            border-radius: 3px;
            font-size: 11px;
            font-family: monospace;
        }
        QPushButton#dialogBtn {
            background-color: #2d2d2d;
            color: #e0e0e0;
            border: 1px solid #3a3a3a;
            padding: 8px 18px;
            border-radius: 4px;
            font-size: 13px;
        }
        QPushButton#dialogBtn:hover {
            background-color: #3d3d3d;
        }
        QPushButton#colorBtn {
            border: 1px solid #4a4a4a;
            border-radius: 3px;
            min-width: 32px;
            min-height: 20px;
            max-width: 32px;
            max-height: 20px;
        }
        QPushButton#colorBtn:hover {
            border: 1px solid #888888;
        }
        QPushButton#presetCard {
            background-color: #252525;
            border: 2px solid #333333;
            border-radius: 6px;
            padding: 6px;
            text-align: left;
        }
        QPushButton#presetCard:hover {
            border: 2px solid #555555;
        }
        QPushButton#presetCardActive {
            background-color: #252525;
            border: 2px solid #00d2ff;
            border-radius: 6px;
            padding: 6px;
            text-align: left;
        }
        QPushButton#saveCurrentBtn {
            background-color: #1e3a2e;
            color: #80f0b0;
            border: 1px solid #2a5a3e;
            border-radius: 4px;
            padding: 6px;
            font-size: 12px;
        }
        QPushButton#saveCurrentBtn:hover {
            background-color: #2a5a3e;
        }
        QScrollBar:vertical {
            background-color: #1e1e1e;
            width: 10px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background-color: #3a3a3a;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #505050;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");
}

void ThemeDialog::rebuildPresetGrid() {
    int scrollPos = m_presetScroll ? m_presetScroll->verticalScrollBar()->value() : 0;

    // Clear existing preset cards
    while (m_presetGrid->count() > 0) {
        QLayoutItem* item = m_presetGrid->takeAt(0);
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    const auto& builtins = ThemeManager::instance().builtinPresets();
    int row = 0, col = 0;

    for (int i = 0; i < builtins.size(); ++i) {
        QWidget* card = createPresetCard(builtins[i], i, false);
        m_presetGrid->addWidget(card, row, col);
        col++;
        if (col >= 2) { col = 0; row++; }
    }

    // Separator
    if (col != 0) { row++; col = 0; }
    QFrame* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #3a3a3a;");
    m_presetGrid->addWidget(sep, row, 0, 1, 2);
    row++;

    // User presets
    const auto& userPresets = ThemeManager::instance().userPresets();
    for (int i = 0; i < userPresets.size(); ++i) {
        QWidget* card = createPresetCard(userPresets[i], i, true);
        m_presetGrid->addWidget(card, row, col);
        col++;
        if (col >= 2) { col = 0; row++; }
    }

    // Save Current button
    if (col != 0) { row++; col = 0; }
    QPushButton* saveBtn = new QPushButton(tr("+ Save Current Theme"), m_presetContainer);
    saveBtn->setObjectName("saveCurrentBtn");
    connect(saveBtn, &QPushButton::clicked, this, &ThemeDialog::onSaveUserPreset);
    m_presetGrid->addWidget(saveBtn, row, 0, 1, 2);

    if (m_presetScroll) {
        QMetaObject::invokeMethod(this, [this, scrollPos]() {
            m_presetScroll->verticalScrollBar()->setValue(scrollPos);
        }, Qt::QueuedConnection);
    }
}

QWidget* ThemeDialog::createPresetCard(const Theme& theme, int index, bool isUserPreset) {
    QPushButton* card = new QPushButton(m_presetContainer);

    bool isActive = (index == m_selectedPresetIndex && isUserPreset == m_selectedIsUser);
    card->setObjectName(isActive ? "presetCardActive" : "presetCard");

    // Build card content: color swatches + name
    QVBoxLayout* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(4, 4, 4, 4);
    cardLayout->setSpacing(4);

    // Color swatch strip
QFrame* swatchRowContainer = new QFrame(card);
    QHBoxLayout* swatchRow = new QHBoxLayout(swatchRowContainer);
    swatchRow->setContentsMargins(0, 0, 0, 0);
    swatchRow->setSpacing(2);
    QColor swatchColors[] = {theme.windowBg, theme.treeItemSelected, theme.progressChunk, theme.buttonBg, theme.textPrimary};
    for (const auto& c : swatchColors) {
        QFrame* swatch = new QFrame(swatchRowContainer);
        swatch->setFixedSize(14, 10);
        swatch->setStyleSheet(QString("background-color: %1; border-radius: 2px; border: 1px solid #444;").arg(c.name()));
        swatchRow->addWidget(swatch);
    }
    swatchRow->addStretch(1);
    cardLayout->addWidget(swatchRowContainer);

    QLabel* nameLabel = new QLabel(theme.name, card);
    nameLabel->setStyleSheet("color: #d0d0d0; font-size: 11px;");
    nameLabel->setWordWrap(true);
    cardLayout->addWidget(nameLabel);

    // Header layout for user presets with title and delete button
    if (isUserPreset) {
        QHBoxLayout* headerRow = new QHBoxLayout();
        headerRow->setContentsMargins(0, 0, 0, 0);
        headerRow->setSpacing(2);

        QLabel* userBadge = new QLabel(tr("[User]"), card);
        userBadge->setStyleSheet("color: #80f0b0; font-size: 10px; font-weight: bold;");
        headerRow->addWidget(userBadge);
        headerRow->addStretch(1);

        QPushButton* delBtn = new QPushButton("✕", card);
        delBtn->setToolTip(tr("Delete this custom saved preset"));
        delBtn->setStyleSheet(R"(
            QPushButton {
                background-color: transparent;
                color: #ff6666;
                border: none;
                font-weight: bold;
                font-size: 12px;
                padding: 0px 2px;
                min-width: 16px;
                max-width: 16px;
                min-height: 16px;
                max-height: 16px;
            }
            QPushButton:hover {
                background-color: #aa0000;
                color: #ffffff;
                border-radius: 8px;
            }
        )");
        headerRow->addWidget(delBtn);
        cardLayout->addLayout(headerRow);

        connect(delBtn, &QPushButton::clicked, this, [this, index]() {
            ThemeManager::instance().deleteUserPreset(index);
            ThemeManager::instance().saveSettings();
            if (m_selectedIsUser && m_selectedPresetIndex == index) {
                m_selectedPresetIndex = 0;
                m_selectedIsUser = false;
                onPresetClicked(0, false);
            } else if (m_selectedIsUser && m_selectedPresetIndex > index) {
                m_selectedPresetIndex--;
            }
            rebuildPresetGrid();
        });
    }

    connect(card, &QPushButton::clicked, this, [this, index, isUserPreset]() {
        onPresetClicked(index, isUserPreset);
    });

    // Right-click context menu for user presets
    if (isUserPreset) {
        card->setContextMenuPolicy(Qt::CustomContextMenu);
        card->setToolTip(tr("Right-click to rename or delete this custom preset"));
        connect(card, &QPushButton::customContextMenuRequested, this, [this, index](const QPoint& pos) {
            QMenu menu;
            menu.setStyleSheet(R"(
                QMenu { background-color: #1e1e1e; color: #e0e0e0; border: 1px solid #333; }
                QMenu::item:selected { background-color: #007acc; }
            )");
            QAction* renameAct = menu.addAction(tr("Rename"));
            QAction* deleteAct = menu.addAction(tr("Delete"));

            QAction* chosen = menu.exec(qobject_cast<QWidget*>(sender())->mapToGlobal(pos));
            if (chosen == renameAct) {
                bool ok;
                QString newName = QInputDialog::getText(this, tr("Rename Preset"),
                    tr("New name:"), QLineEdit::Normal,
                    ThemeManager::instance().userPresets()[index].name, &ok);
                if (ok && !newName.isEmpty()) {
                    ThemeManager::instance().renameUserPreset(index, newName);
                    ThemeManager::instance().saveSettings();
                    rebuildPresetGrid();
                }
            } else if (chosen == deleteAct) {
                ThemeManager::instance().deleteUserPreset(index);
                ThemeManager::instance().saveSettings();
                if (m_selectedIsUser && m_selectedPresetIndex == index) {
                    m_selectedPresetIndex = 0;
                    m_selectedIsUser = false;
                    onPresetClicked(0, false);
                } else if (m_selectedIsUser && m_selectedPresetIndex > index) {
                    m_selectedPresetIndex--;
                }
                rebuildPresetGrid();
            }
        });
    } else {
        card->setToolTip(tr("Built-in preset (cannot be removed)"));
    }

    return card;
}

void ThemeDialog::addColorSection(QVBoxLayout* layout, const QString& title) {
    QLabel* header = new QLabel(title);
    header->setObjectName("sectionHeader");
    layout->addWidget(header);
}

static QString getFieldHelpText(const QString& fieldName) {
    if (fieldName == "windowBg") return "Changes the main background color of the entire window area.";
    if (fieldName == "panelBg") return "Changes the background color of inner panels and containers.";
    if (fieldName == "hudBg") return "Changes the background color of the top bar with path inputs and search tools.";
    if (fieldName == "hudBorder") return "Changes the border line color around the top bar.";
    if (fieldName == "inputBg") return "Changes the background color inside text entry boxes (like the folder path box).";
    if (fieldName == "inputBorder") return "Changes the border color around text entry boxes.";
    if (fieldName == "inputFocusBorder") return "Changes the highlight border color when you click inside a text box.";
    if (fieldName == "buttonBg") return "Changes the normal background color of action buttons (like Scan or Cancel).";
    if (fieldName == "buttonHoverBg") return "Changes the button background color when you hover your mouse pointer over it.";
    if (fieldName == "buttonPressedBg") return "Changes the button background color when you click down on it.";
    if (fieldName == "buttonBorder") return "Changes the outline border color around buttons.";
    if (fieldName == "buttonText") return "Changes the color of the text written inside buttons.";
    if (fieldName == "textPrimary") return "Changes the primary readable text color used across the app.";
    if (fieldName == "textSecondary") return "Changes the color of secondary text like status hints and info labels.";
    if (fieldName == "treeBg") return "Changes the background color of the file and folder tree list.";
    if (fieldName == "treeAltBg") return "Changes every alternating row background color in the file tree list.";
    if (fieldName == "treeItemHover") return "Changes the row background color when hovering over a folder or file.";
    if (fieldName == "treeItemSelected") return "Changes the background color of the currently selected folder or file.";
    if (fieldName == "treeItemSelectedText") return "Changes the text color of the currently selected folder or file.";
    if (fieldName == "percentBarColor") return "Changes the color of the horizontal bar showing folder size ratio.";
    if (fieldName == "headerBg") return "Changes the background color of table column headers (Name, Size, Items).";
    if (fieldName == "headerText") return "Changes the text color in table column headers.";
    if (fieldName == "headerBorder") return "Changes the separator line color between column headers.";
    if (fieldName == "progressBg") return "Changes the background track color of the scanning progress bar.";
    if (fieldName == "progressChunk") return "Changes the animated fill color of the scanning progress bar.";
    if (fieldName == "progressText") return "Changes the text color showing percentage inside the progress bar.";
    if (fieldName == "progressBorder") return "Changes the outline border color of the progress bar.";
    if (fieldName == "menuBg") return "Changes the background color of right-click context menus.";
    if (fieldName == "menuText") return "Changes the text color of actions in right-click menus.";
    if (fieldName == "menuSelectedBg") return "Changes the highlight color when hovering over a menu action.";
    if (fieldName == "menuBorder") return "Changes the border outline around right-click menus.";
    if (fieldName == "deleteEnabledBg") return "Changes the background color of the 'Enable Delete' button when active.";
    if (fieldName == "deleteEnabledBorder") return "Changes the border color of the active 'Enable Delete' button.";
    return "Click the color box or type a hex code to customize this visual element.";
}

void ThemeDialog::addColorRow(QVBoxLayout* layout, const QString& label, const QString& fieldName) {
    QHBoxLayout* row = new QHBoxLayout();
    row->setSpacing(8);

    QLabel* lbl = new QLabel(label);
    lbl->setMinimumWidth(130);
    lbl->setMaximumWidth(150);
    row->addWidget(lbl);

    QPushButton* helpBtn = new QPushButton("?", this);
    helpBtn->setObjectName("helpIconBtn");
    helpBtn->setCursor(Qt::PointingHandCursor);
    QString helpTip = getFieldHelpText(fieldName);
    helpBtn->setToolTip(helpTip);
    row->addWidget(helpBtn);

    lbl->setToolTip(helpTip);

    QPushButton* colorBtn = new QPushButton();
    colorBtn->setObjectName("colorBtn");
    colorBtn->setToolTip(helpTip);
    QColor currentColor = getColorRef(fieldName);
    colorBtn->setStyleSheet(QString("background-color: %1;").arg(currentColor.name(QColor::HexArgb)));
    row->addWidget(colorBtn);

    QLineEdit* hexEdit = new QLineEdit(currentColor.name(QColor::HexArgb));
    hexEdit->setFixedWidth(90);
    hexEdit->setMaxLength(9); // #AARRGGBB
    hexEdit->setToolTip(helpTip);
    row->addWidget(hexEdit);
    row->addStretch(1);

    m_colorButtons[fieldName] = colorBtn;
    m_hexInputs[fieldName] = hexEdit;

    auto showHelp = [this, label, helpTip]() {
        if (m_helpBanner) {
            m_helpBanner->setText(QString("💡 <b>%1</b>: %2").arg(label, helpTip));
        }
    };

    connect(helpBtn, &QPushButton::clicked, this, [this, label, helpTip, showHelp]() {
        showHelp();
        QMessageBox msg(this);
        msg.setWindowTitle(tr("What is %1?").arg(label));
        msg.setText(QString("<font color='#00d2ff'><b>%1</b></font><br><br><font color='#e0e0e0'>%2</font>").arg(label, helpTip));
        msg.setStyleSheet("QMessageBox { background-color: #1a1a1a; color: #e0e0e0; } QPushButton { background-color: #2d2d2d; color: #ffffff; border: 1px solid #444; padding: 6px 16px; border-radius: 4px; }");
        msg.exec();
    });

    connect(colorBtn, &QPushButton::clicked, this, [this, fieldName, showHelp]() {
        showHelp();
        onColorButtonClicked(fieldName);
    });

    connect(hexEdit, &QLineEdit::editingFinished, this, [this, fieldName]() {
        onHexEditFinished(fieldName);
    });

    layout->addLayout(row);
}

void ThemeDialog::loadThemeIntoEditor(const Theme& theme) {
    m_editingTheme = theme;

    // Update all color buttons and hex inputs
    QStringList fields = m_colorButtons.keys();
    for (const auto& f : fields) {
        QColor c = getColorRef(f);
        updateColorButton(f, c);
        if (m_hexInputs.contains(f)) {
            m_hexInputs[f]->setText(c.name(QColor::HexArgb));
        }
    }
}

void ThemeDialog::applyEditingTheme() {
    ThemeManager::instance().applyTheme(m_editingTheme);
    if (parentWidget()) {
        parentWidget()->setStyleSheet(ThemeManager::instance().generateStylesheet(m_editingTheme));
    }
}

void ThemeDialog::updateColorButton(const QString& fieldName, const QColor& color) {
    if (m_colorButtons.contains(fieldName)) {
        m_colorButtons[fieldName]->setStyleSheet(
            QString("background-color: %1;").arg(color.name(QColor::HexArgb))
        );
    }
}

QColor& ThemeDialog::getColorRef(const QString& f) {
    if (f == "windowBg") return m_editingTheme.windowBg;
    if (f == "panelBg") return m_editingTheme.panelBg;
    if (f == "hudBg") return m_editingTheme.hudBg;
    if (f == "hudBorder") return m_editingTheme.hudBorder;
    if (f == "inputBg") return m_editingTheme.inputBg;
    if (f == "inputBorder") return m_editingTheme.inputBorder;
    if (f == "inputFocusBorder") return m_editingTheme.inputFocusBorder;
    if (f == "buttonBg") return m_editingTheme.buttonBg;
    if (f == "buttonHoverBg") return m_editingTheme.buttonHoverBg;
    if (f == "buttonPressedBg") return m_editingTheme.buttonPressedBg;
    if (f == "buttonBorder") return m_editingTheme.buttonBorder;
    if (f == "buttonText") return m_editingTheme.buttonText;
    if (f == "textPrimary") return m_editingTheme.textPrimary;
    if (f == "textSecondary") return m_editingTheme.textSecondary;
    if (f == "treeBg") return m_editingTheme.treeBg;
    if (f == "treeAltBg") return m_editingTheme.treeAltBg;
    if (f == "treeItemHover") return m_editingTheme.treeItemHover;
    if (f == "treeItemSelected") return m_editingTheme.treeItemSelected;
    if (f == "treeItemSelectedText") return m_editingTheme.treeItemSelectedText;
    if (f == "percentBarColor") return m_editingTheme.percentBarColor;
    if (f == "headerBg") return m_editingTheme.headerBg;
    if (f == "headerText") return m_editingTheme.headerText;
    if (f == "headerBorder") return m_editingTheme.headerBorder;
    if (f == "progressBg") return m_editingTheme.progressBg;
    if (f == "progressChunk") return m_editingTheme.progressChunk;
    if (f == "progressText") return m_editingTheme.progressText;
    if (f == "progressBorder") return m_editingTheme.progressBorder;
    if (f == "menuBg") return m_editingTheme.menuBg;
    if (f == "menuText") return m_editingTheme.menuText;
    if (f == "menuSelectedBg") return m_editingTheme.menuSelectedBg;
    if (f == "menuBorder") return m_editingTheme.menuBorder;
    if (f == "deleteEnabledBg") return m_editingTheme.deleteEnabledBg;
    if (f == "deleteEnabledBorder") return m_editingTheme.deleteEnabledBorder;
    // Fallback (should never happen)
    return m_editingTheme.windowBg;
}

void ThemeDialog::onPresetClicked(int index, bool isUserPreset) {
    m_selectedPresetIndex = index;
    m_selectedIsUser = isUserPreset;

    const Theme& theme = isUserPreset
        ? ThemeManager::instance().userPresets()[index]
        : ThemeManager::instance().builtinPresets()[index];

    loadThemeIntoEditor(theme);
    applyEditingTheme();

    if (isUserPreset) {
        ThemeManager::instance().applyUserPreset(index);
    } else {
        ThemeManager::instance().applyBuiltinPreset(index);
    }

    rebuildPresetGrid();
}

void ThemeDialog::onColorButtonClicked(const QString& fieldName) {
    QColor current = getColorRef(fieldName);
    QColor chosen = QColorDialog::getColor(current, this, tr("Pick Color"),
                                            QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        getColorRef(fieldName) = chosen;
        updateColorButton(fieldName, chosen);
        if (m_hexInputs.contains(fieldName)) {
            m_hexInputs[fieldName]->setText(chosen.name(QColor::HexArgb));
        }
        applyEditingTheme();
    }
}

void ThemeDialog::onHexEditFinished(const QString& fieldName) {
    if (!m_hexInputs.contains(fieldName)) return;
    QString hex = m_hexInputs[fieldName]->text().trimmed();
    QColor c(hex);
    if (c.isValid()) {
        getColorRef(fieldName) = c;
        updateColorButton(fieldName, c);
        applyEditingTheme();
    }
}

void ThemeDialog::onSaveUserPreset() {
    int idx = ThemeManager::instance().saveUserPreset(m_editingTheme);
    m_selectedPresetIndex = idx;
    m_selectedIsUser = true;
    ThemeManager::instance().applyUserPreset(idx);
    ThemeManager::instance().saveSettings();
    rebuildPresetGrid();
}

void ThemeDialog::onReset() {
    m_editingTheme = m_originalTheme;
    loadThemeIntoEditor(m_originalTheme);
    applyEditingTheme();
    // Reset to original selection
    m_selectedPresetIndex = ThemeManager::instance().activePresetIndex();
    m_selectedIsUser = ThemeManager::instance().isActiveUserPreset();
    rebuildPresetGrid();
}
