#include "theme_manager.hpp"
#include <QJsonDocument>

ThemeManager::ThemeManager() {
    initBuiltinPresets();
    m_activeTheme = m_builtinPresets[0];
}

ThemeManager& ThemeManager::instance() {
    static ThemeManager inst;
    return inst;
}

const QVector<Theme>& ThemeManager::builtinPresets() const { return m_builtinPresets; }
const QVector<Theme>& ThemeManager::userPresets() const { return m_userPresets; }
const Theme& ThemeManager::activeTheme() const { return m_activeTheme; }
int ThemeManager::activePresetIndex() const { return m_activePresetIndex; }
bool ThemeManager::isActiveUserPreset() const { return m_isUserPreset; }

void ThemeManager::applyBuiltinPreset(int index) {
    if (index < 0 || index >= m_builtinPresets.size()) return;
    m_activePresetIndex = index;
    m_isUserPreset = false;
    m_activeTheme = m_builtinPresets[index];
    emit themeChanged();
}

void ThemeManager::applyUserPreset(int index) {
    if (index < 0 || index >= m_userPresets.size()) return;
    m_activePresetIndex = index;
    m_isUserPreset = true;
    m_activeTheme = m_userPresets[index];
    emit themeChanged();
}

void ThemeManager::applyTheme(const Theme& theme) {
    m_activeTheme = theme;
    emit themeChanged();
}

int ThemeManager::saveUserPreset(const Theme& theme) {
    Theme t = theme;
    if (t.name.isEmpty()) {
        t.name = QString("User Preset %1").arg(m_userPresets.size() + 1);
    }
    m_userPresets.append(t);
    return m_userPresets.size() - 1;
}

void ThemeManager::deleteUserPreset(int index) {
    if (index < 0 || index >= m_userPresets.size()) return;
    m_userPresets.removeAt(index);
}

void ThemeManager::renameUserPreset(int index, const QString& name) {
    if (index < 0 || index >= m_userPresets.size()) return;
    m_userPresets[index].name = name;
}

QJsonObject ThemeManager::themeToJson(const Theme& t) const {
    QJsonObject obj;
    obj["name"] = t.name;
    auto c2s = [](const QColor& c) { return c.name(QColor::HexArgb); };
    obj["windowBg"] = c2s(t.windowBg);
    obj["panelBg"] = c2s(t.panelBg);
    obj["hudBg"] = c2s(t.hudBg);
    obj["hudBorder"] = c2s(t.hudBorder);
    obj["inputBg"] = c2s(t.inputBg);
    obj["inputBorder"] = c2s(t.inputBorder);
    obj["inputFocusBorder"] = c2s(t.inputFocusBorder);
    obj["buttonBg"] = c2s(t.buttonBg);
    obj["buttonHoverBg"] = c2s(t.buttonHoverBg);
    obj["buttonPressedBg"] = c2s(t.buttonPressedBg);
    obj["buttonBorder"] = c2s(t.buttonBorder);
    obj["buttonText"] = c2s(t.buttonText);
    obj["textPrimary"] = c2s(t.textPrimary);
    obj["textSecondary"] = c2s(t.textSecondary);
    obj["treeBg"] = c2s(t.treeBg);
    obj["treeAltBg"] = c2s(t.treeAltBg);
    obj["treeItemHover"] = c2s(t.treeItemHover);
    obj["treeItemSelected"] = c2s(t.treeItemSelected);
    obj["treeItemSelectedText"] = c2s(t.treeItemSelectedText);
    obj["percentBarColor"] = c2s(t.percentBarColor);
    obj["headerBg"] = c2s(t.headerBg);
    obj["headerText"] = c2s(t.headerText);
    obj["headerBorder"] = c2s(t.headerBorder);
    obj["progressBg"] = c2s(t.progressBg);
    obj["progressChunk"] = c2s(t.progressChunk);
    obj["progressText"] = c2s(t.progressText);
    obj["progressBorder"] = c2s(t.progressBorder);
    obj["menuBg"] = c2s(t.menuBg);
    obj["menuText"] = c2s(t.menuText);
    obj["menuSelectedBg"] = c2s(t.menuSelectedBg);
    obj["menuBorder"] = c2s(t.menuBorder);
    obj["deleteEnabledBg"] = c2s(t.deleteEnabledBg);
    obj["deleteEnabledBorder"] = c2s(t.deleteEnabledBorder);
    return obj;
}

Theme ThemeManager::themeFromJson(const QJsonObject& obj) const {
    Theme t;
    t.name = obj["name"].toString();
    auto s2c = [](const QString& s) { return QColor(s); };
    t.windowBg = s2c(obj["windowBg"].toString());
    t.panelBg = s2c(obj["panelBg"].toString());
    t.hudBg = s2c(obj["hudBg"].toString());
    t.hudBorder = s2c(obj["hudBorder"].toString());
    t.inputBg = s2c(obj["inputBg"].toString());
    t.inputBorder = s2c(obj["inputBorder"].toString());
    t.inputFocusBorder = s2c(obj["inputFocusBorder"].toString());
    t.buttonBg = s2c(obj["buttonBg"].toString());
    t.buttonHoverBg = s2c(obj["buttonHoverBg"].toString());
    t.buttonPressedBg = s2c(obj["buttonPressedBg"].toString());
    t.buttonBorder = s2c(obj["buttonBorder"].toString());
    t.buttonText = s2c(obj["buttonText"].toString());
    t.textPrimary = s2c(obj["textPrimary"].toString());
    t.textSecondary = s2c(obj["textSecondary"].toString());
    t.treeBg = s2c(obj["treeBg"].toString());
    t.treeAltBg = s2c(obj["treeAltBg"].toString());
    t.treeItemHover = s2c(obj["treeItemHover"].toString());
    t.treeItemSelected = s2c(obj["treeItemSelected"].toString());
    t.treeItemSelectedText = s2c(obj["treeItemSelectedText"].toString());
    t.percentBarColor = s2c(obj["percentBarColor"].toString());
    t.headerBg = s2c(obj["headerBg"].toString());
    t.headerText = s2c(obj["headerText"].toString());
    t.headerBorder = s2c(obj["headerBorder"].toString());
    t.progressBg = s2c(obj["progressBg"].toString());
    t.progressChunk = s2c(obj["progressChunk"].toString());
    t.progressText = s2c(obj["progressText"].toString());
    t.progressBorder = s2c(obj["progressBorder"].toString());
    t.menuBg = s2c(obj["menuBg"].toString());
    t.menuText = s2c(obj["menuText"].toString());
    t.menuSelectedBg = s2c(obj["menuSelectedBg"].toString());
    t.menuBorder = s2c(obj["menuBorder"].toString());
    t.deleteEnabledBg = s2c(obj["deleteEnabledBg"].toString());
    t.deleteEnabledBorder = s2c(obj["deleteEnabledBorder"].toString());
    return t;
}

void ThemeManager::saveSettings() const {
    QSettings settings("SpaceMap", "SpaceMap");
    settings.setValue("theme/activeIndex", m_activePresetIndex);
    settings.setValue("theme/isUserPreset", m_isUserPreset);

    QJsonArray arr;
    for (const auto& t : m_userPresets) {
        arr.append(themeToJson(t));
    }
    settings.setValue("theme/userPresets", QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void ThemeManager::loadSettings() {
    QSettings settings("SpaceMap", "SpaceMap");
    int idx = settings.value("theme/activeIndex", 0).toInt();
    bool isUser = settings.value("theme/isUserPreset", false).toBool();

    QString presetsStr = settings.value("theme/userPresets").toString();
    if (!presetsStr.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(presetsStr.toUtf8());
        if (doc.isArray()) {
            m_userPresets.clear();
            for (const auto& val : doc.array()) {
                m_userPresets.append(themeFromJson(val.toObject()));
            }
        }
    }

    if (isUser && idx >= 0 && idx < m_userPresets.size()) {
        applyUserPreset(idx);
    } else if (!isUser && idx >= 0 && idx < m_builtinPresets.size()) {
        applyBuiltinPreset(idx);
    } else {
        applyBuiltinPreset(0);
    }
}

QString ThemeManager::generateStylesheet(const Theme& t) const {
    auto cn = [](const QColor& c) { return c.name(QColor::HexArgb); };
    auto hex = [](const QColor& c) { return c.name(); };

    return QString(R"(
        QMainWindow {
            background-color: %1;
        }
        QLabel {
            color: %2;
            font-size: 13px;
        }
        QProgressBar {
            background-color: %3;
            color: %4;
            border: 1px solid %5;
            border-radius: 4px;
            text-align: center;
            height: 18px;
            font-size: 11px;
            font-weight: bold;
        }
        QProgressBar::chunk {
            background-color: %6;
            border-radius: 3px;
        }
        QLineEdit {
            background-color: %7;
            color: %8;
            border: 1px solid %9;
            padding: 6px;
            border-radius: 4px;
            font-size: 13px;
        }
        QLineEdit:focus {
            border: 1px solid %10;
        }
        QComboBox {
            background-color: %11;
            color: %12;
            border: 1px solid %13;
            padding: 6px 20px 6px 10px;
            border-radius: 4px;
            font-size: 13px;
            min-width: 180px;
        }
        QComboBox:focus {
            border: 1px solid %14;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left-width: 1px;
            border-left-color: %15;
            border-left-style: solid;
            border-top-right-radius: 3px;
            border-bottom-right-radius: 3px;
        }
        QComboBox QAbstractItemView {
            background-color: %16;
            color: %17;
            border: 1px solid %18;
            selection-background-color: %19;
            selection-color: %20;
        }
        QPushButton {
            background-color: %21;
            color: %22;
            border: 1px solid %23;
            padding: 6px 12px;
            border-radius: 4px;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: %24;
            border: 1px solid %25;
        }
        QPushButton:pressed {
            background-color: %26;
        }
        QTreeView {
            background-color: %27;
            color: %28;
            border: 1px solid %29;
            border-radius: 4px;
            alternate-background-color: %30;
        }
        QTreeView::item:hover {
            background-color: %31;
        }
        QTreeView::item:selected {
            background-color: %32;
            color: %33;
        }
        QHeaderView::section {
            background-color: %34;
            color: %35;
            border: 1px solid %36;
            padding: 4px;
        }
        #hudFrame {
            background-color: %37;
            border: 1px solid %38;
            border-radius: 4px;
        }
    )")
    .arg(hex(t.windowBg))        // 1
    .arg(hex(t.textPrimary))     // 2
    .arg(hex(t.progressBg))      // 3
    .arg(hex(t.progressText))    // 4
    .arg(hex(t.progressBorder))  // 5
    .arg(hex(t.progressChunk))   // 6
    .arg(hex(t.inputBg))         // 7
    .arg(hex(t.textPrimary))     // 8
    .arg(hex(t.inputBorder))     // 9
    .arg(hex(t.inputFocusBorder))// 10
    .arg(hex(t.inputBg))         // 11
    .arg(hex(t.textPrimary))     // 12
    .arg(hex(t.inputBorder))     // 13
    .arg(hex(t.inputFocusBorder))// 14
    .arg(hex(t.inputBorder))     // 15
    .arg(hex(t.inputBg))         // 16
    .arg(hex(t.textPrimary))     // 17
    .arg(hex(t.inputBorder))     // 18
    .arg(hex(t.treeItemSelected))// 19
    .arg(hex(t.treeItemSelectedText)) // 20
    .arg(hex(t.buttonBg))        // 21
    .arg(hex(t.buttonText))      // 22
    .arg(hex(t.buttonBorder))    // 23
    .arg(hex(t.buttonHoverBg))   // 24
    .arg(QString(hex(t.buttonBorder)).replace('#', '#').left(1) == '#' ? hex(t.buttonHoverBg) : hex(t.buttonHoverBg)) // 25 - button hover border
    .arg(hex(t.buttonPressedBg)) // 26
    .arg(hex(t.treeBg))          // 27
    .arg(hex(t.textPrimary))     // 28 - tree text
    .arg(hex(t.hudBorder))       // 29 - tree border (reuse panel border)
    .arg(hex(t.treeAltBg))       // 30
    .arg(hex(t.treeItemHover))   // 31
    .arg(hex(t.treeItemSelected))// 32
    .arg(hex(t.treeItemSelectedText)) // 33
    .arg(hex(t.headerBg))        // 34
    .arg(hex(t.headerText))      // 35
    .arg(hex(t.headerBorder))    // 36
    .arg(hex(t.hudBg))           // 37
    .arg(hex(t.hudBorder));      // 38
}

// Helper macro to reduce repetition for preset initialization
#define PRESET(NAME) do { Theme t; t.name = NAME;
#define END_PRESET m_builtinPresets.append(t); } while(0)

void ThemeManager::initBuiltinPresets() {
    m_builtinPresets.clear();

    // 1. Midnight (default - matches existing dark theme)
    { Theme t; t.name = "Midnight";
      t.windowBg = QColor("#121212"); t.panelBg = QColor("#1c1c1c");
      t.hudBg = QColor("#1c1c1c"); t.hudBorder = QColor("#2d2d2d");
      t.inputBg = QColor("#1e1e1e"); t.inputBorder = QColor("#333333"); t.inputFocusBorder = QColor("#007acc");
      t.buttonBg = QColor("#2d2d2d"); t.buttonHoverBg = QColor("#3d3d3d"); t.buttonPressedBg = QColor("#005999");
      t.buttonBorder = QColor("#3a3a3a"); t.buttonText = QColor("#e0e0e0");
      t.textPrimary = QColor("#ffffff"); t.textSecondary = QColor("#888888");
      t.treeBg = QColor("#1a1a1a"); t.treeAltBg = QColor("#222222"); t.treeItemHover = QColor("#2a2a2a");
      t.treeItemSelected = QColor("#007acc"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(70, 130, 180, 120);
      t.headerBg = QColor("#252526"); t.headerText = QColor("#d0d0d0"); t.headerBorder = QColor("#1a1a1a");
      t.progressBg = QColor("#1a1a1a"); t.progressChunk = QColor("#00d2ff"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#2d2d2d");
      t.menuBg = QColor("#1e1e1e"); t.menuText = QColor("#ffffff"); t.menuSelectedBg = QColor("#007acc"); t.menuBorder = QColor("#333333");
      t.deleteEnabledBg = QColor("#ff3333"); t.deleteEnabledBorder = QColor("#ff6666");
      m_builtinPresets.append(t); }

    // 2. Obsidian
    { Theme t; t.name = "Obsidian";
      t.windowBg = QColor("#0e0e0e"); t.panelBg = QColor("#181818");
      t.hudBg = QColor("#181818"); t.hudBorder = QColor("#2a2a2a");
      t.inputBg = QColor("#1a1a1a"); t.inputBorder = QColor("#303030"); t.inputFocusBorder = QColor("#FFB300");
      t.buttonBg = QColor("#262626"); t.buttonHoverBg = QColor("#363636"); t.buttonPressedBg = QColor("#b37d00");
      t.buttonBorder = QColor("#383838"); t.buttonText = QColor("#e0d0b0");
      t.textPrimary = QColor("#f0e6d0"); t.textSecondary = QColor("#8a7e68");
      t.treeBg = QColor("#141414"); t.treeAltBg = QColor("#1c1c1c"); t.treeItemHover = QColor("#252520");
      t.treeItemSelected = QColor("#FFB300"); t.treeItemSelectedText = QColor("#0e0e0e");
      t.percentBarColor = QColor(255, 179, 0, 100);
      t.headerBg = QColor("#1e1e1a"); t.headerText = QColor("#d4c8a8"); t.headerBorder = QColor("#141410");
      t.progressBg = QColor("#1a1a1a"); t.progressChunk = QColor("#FFB300"); t.progressText = QColor("#f0e6d0"); t.progressBorder = QColor("#2a2a2a");
      t.menuBg = QColor("#1a1a1a"); t.menuText = QColor("#f0e6d0"); t.menuSelectedBg = QColor("#FFB300"); t.menuBorder = QColor("#303030");
      t.deleteEnabledBg = QColor("#cc3300"); t.deleteEnabledBorder = QColor("#ff5533");
      m_builtinPresets.append(t); }

    // 3. Dracula
    { Theme t; t.name = "Dracula";
      t.windowBg = QColor("#282a36"); t.panelBg = QColor("#21222c");
      t.hudBg = QColor("#21222c"); t.hudBorder = QColor("#44475a");
      t.inputBg = QColor("#313244"); t.inputBorder = QColor("#44475a"); t.inputFocusBorder = QColor("#bd93f9");
      t.buttonBg = QColor("#44475a"); t.buttonHoverBg = QColor("#565972"); t.buttonPressedBg = QColor("#7c5cbf");
      t.buttonBorder = QColor("#555870"); t.buttonText = QColor("#f8f8f2");
      t.textPrimary = QColor("#f8f8f2"); t.textSecondary = QColor("#6272a4");
      t.treeBg = QColor("#282a36"); t.treeAltBg = QColor("#2e3042"); t.treeItemHover = QColor("#363848");
      t.treeItemSelected = QColor("#bd93f9"); t.treeItemSelectedText = QColor("#282a36");
      t.percentBarColor = QColor(189, 147, 249, 100);
      t.headerBg = QColor("#2e303e"); t.headerText = QColor("#f8f8f2"); t.headerBorder = QColor("#21222c");
      t.progressBg = QColor("#21222c"); t.progressChunk = QColor("#ff79c6"); t.progressText = QColor("#f8f8f2"); t.progressBorder = QColor("#44475a");
      t.menuBg = QColor("#282a36"); t.menuText = QColor("#f8f8f2"); t.menuSelectedBg = QColor("#bd93f9"); t.menuBorder = QColor("#44475a");
      t.deleteEnabledBg = QColor("#ff5555"); t.deleteEnabledBorder = QColor("#ff7777");
      m_builtinPresets.append(t); }

    // 4. Nord
    { Theme t; t.name = "Nord";
      t.windowBg = QColor("#2e3440"); t.panelBg = QColor("#3b4252");
      t.hudBg = QColor("#3b4252"); t.hudBorder = QColor("#4c566a");
      t.inputBg = QColor("#3b4252"); t.inputBorder = QColor("#4c566a"); t.inputFocusBorder = QColor("#88c0d0");
      t.buttonBg = QColor("#434c5e"); t.buttonHoverBg = QColor("#4c566a"); t.buttonPressedBg = QColor("#5e81ac");
      t.buttonBorder = QColor("#4c566a"); t.buttonText = QColor("#eceff4");
      t.textPrimary = QColor("#eceff4"); t.textSecondary = QColor("#d8dee9");
      t.treeBg = QColor("#2e3440"); t.treeAltBg = QColor("#353b49"); t.treeItemHover = QColor("#3b4252");
      t.treeItemSelected = QColor("#88c0d0"); t.treeItemSelectedText = QColor("#2e3440");
      t.percentBarColor = QColor(136, 192, 208, 100);
      t.headerBg = QColor("#3b4252"); t.headerText = QColor("#d8dee9"); t.headerBorder = QColor("#2e3440");
      t.progressBg = QColor("#3b4252"); t.progressChunk = QColor("#a3be8c"); t.progressText = QColor("#eceff4"); t.progressBorder = QColor("#4c566a");
      t.menuBg = QColor("#3b4252"); t.menuText = QColor("#eceff4"); t.menuSelectedBg = QColor("#88c0d0"); t.menuBorder = QColor("#4c566a");
      t.deleteEnabledBg = QColor("#bf616a"); t.deleteEnabledBorder = QColor("#d08770");
      m_builtinPresets.append(t); }

    // 5. Solarized Dark
    { Theme t; t.name = "Solarized Dark";
      t.windowBg = QColor("#002b36"); t.panelBg = QColor("#073642");
      t.hudBg = QColor("#073642"); t.hudBorder = QColor("#586e75");
      t.inputBg = QColor("#073642"); t.inputBorder = QColor("#586e75"); t.inputFocusBorder = QColor("#268bd2");
      t.buttonBg = QColor("#094152"); t.buttonHoverBg = QColor("#0b5268"); t.buttonPressedBg = QColor("#1a6d99");
      t.buttonBorder = QColor("#586e75"); t.buttonText = QColor("#93a1a1");
      t.textPrimary = QColor("#93a1a1"); t.textSecondary = QColor("#657b83");
      t.treeBg = QColor("#002b36"); t.treeAltBg = QColor("#03313e"); t.treeItemHover = QColor("#073642");
      t.treeItemSelected = QColor("#268bd2"); t.treeItemSelectedText = QColor("#fdf6e3");
      t.percentBarColor = QColor(38, 139, 210, 100);
      t.headerBg = QColor("#073642"); t.headerText = QColor("#839496"); t.headerBorder = QColor("#002b36");
      t.progressBg = QColor("#073642"); t.progressChunk = QColor("#859900"); t.progressText = QColor("#93a1a1"); t.progressBorder = QColor("#586e75");
      t.menuBg = QColor("#073642"); t.menuText = QColor("#93a1a1"); t.menuSelectedBg = QColor("#268bd2"); t.menuBorder = QColor("#586e75");
      t.deleteEnabledBg = QColor("#dc322f"); t.deleteEnabledBorder = QColor("#cb4b16");
      m_builtinPresets.append(t); }

    // 6. Monokai
    { Theme t; t.name = "Monokai";
      t.windowBg = QColor("#272822"); t.panelBg = QColor("#1e1f1c");
      t.hudBg = QColor("#1e1f1c"); t.hudBorder = QColor("#3e3d32");
      t.inputBg = QColor("#3e3d32"); t.inputBorder = QColor("#49483e"); t.inputFocusBorder = QColor("#f92672");
      t.buttonBg = QColor("#3e3d32"); t.buttonHoverBg = QColor("#4e4d42"); t.buttonPressedBg = QColor("#c41958");
      t.buttonBorder = QColor("#49483e"); t.buttonText = QColor("#f8f8f2");
      t.textPrimary = QColor("#f8f8f2"); t.textSecondary = QColor("#75715e");
      t.treeBg = QColor("#272822"); t.treeAltBg = QColor("#2d2e28"); t.treeItemHover = QColor("#3e3d32");
      t.treeItemSelected = QColor("#f92672"); t.treeItemSelectedText = QColor("#f8f8f2");
      t.percentBarColor = QColor(166, 226, 46, 100);
      t.headerBg = QColor("#2d2e28"); t.headerText = QColor("#f8f8f2"); t.headerBorder = QColor("#272822");
      t.progressBg = QColor("#3e3d32"); t.progressChunk = QColor("#a6e22e"); t.progressText = QColor("#f8f8f2"); t.progressBorder = QColor("#49483e");
      t.menuBg = QColor("#272822"); t.menuText = QColor("#f8f8f2"); t.menuSelectedBg = QColor("#f92672"); t.menuBorder = QColor("#49483e");
      t.deleteEnabledBg = QColor("#f92672"); t.deleteEnabledBorder = QColor("#fd5da8");
      m_builtinPresets.append(t); }

    // 7. Gruvbox Dark
    { Theme t; t.name = "Gruvbox Dark";
      t.windowBg = QColor("#282828"); t.panelBg = QColor("#1d2021");
      t.hudBg = QColor("#1d2021"); t.hudBorder = QColor("#3c3836");
      t.inputBg = QColor("#3c3836"); t.inputBorder = QColor("#504945"); t.inputFocusBorder = QColor("#fe8019");
      t.buttonBg = QColor("#3c3836"); t.buttonHoverBg = QColor("#504945"); t.buttonPressedBg = QColor("#d65d0e");
      t.buttonBorder = QColor("#504945"); t.buttonText = QColor("#ebdbb2");
      t.textPrimary = QColor("#ebdbb2"); t.textSecondary = QColor("#a89984");
      t.treeBg = QColor("#282828"); t.treeAltBg = QColor("#2e2c2a"); t.treeItemHover = QColor("#3c3836");
      t.treeItemSelected = QColor("#fe8019"); t.treeItemSelectedText = QColor("#282828");
      t.percentBarColor = QColor(254, 128, 25, 100);
      t.headerBg = QColor("#3c3836"); t.headerText = QColor("#d5c4a1"); t.headerBorder = QColor("#282828");
      t.progressBg = QColor("#3c3836"); t.progressChunk = QColor("#fabd2f"); t.progressText = QColor("#ebdbb2"); t.progressBorder = QColor("#504945");
      t.menuBg = QColor("#3c3836"); t.menuText = QColor("#ebdbb2"); t.menuSelectedBg = QColor("#fe8019"); t.menuBorder = QColor("#504945");
      t.deleteEnabledBg = QColor("#cc241d"); t.deleteEnabledBorder = QColor("#fb4934");
      m_builtinPresets.append(t); }

    // 8. Cyberpunk
    { Theme t; t.name = "Cyberpunk";
      t.windowBg = QColor("#0a0a0f"); t.panelBg = QColor("#0f0f18");
      t.hudBg = QColor("#0f0f18"); t.hudBorder = QColor("#1a1a2e");
      t.inputBg = QColor("#12121f"); t.inputBorder = QColor("#2a1a3e"); t.inputFocusBorder = QColor("#ff2e97");
      t.buttonBg = QColor("#1a1a2e"); t.buttonHoverBg = QColor("#2a1a40"); t.buttonPressedBg = QColor("#cc1a7a");
      t.buttonBorder = QColor("#3a1a50"); t.buttonText = QColor("#e0d0ff");
      t.textPrimary = QColor("#e0d0ff"); t.textSecondary = QColor("#6a5a8a");
      t.treeBg = QColor("#0a0a12"); t.treeAltBg = QColor("#0f0f1a"); t.treeItemHover = QColor("#1a1a2e");
      t.treeItemSelected = QColor("#ff2e97"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(255, 46, 151, 90);
      t.headerBg = QColor("#12121f"); t.headerText = QColor("#c0b0e0"); t.headerBorder = QColor("#0a0a0f");
      t.progressBg = QColor("#0f0f18"); t.progressChunk = QColor("#00fff9"); t.progressText = QColor("#e0d0ff"); t.progressBorder = QColor("#1a1a2e");
      t.menuBg = QColor("#0f0f18"); t.menuText = QColor("#e0d0ff"); t.menuSelectedBg = QColor("#ff2e97"); t.menuBorder = QColor("#2a1a3e");
      t.deleteEnabledBg = QColor("#ff0060"); t.deleteEnabledBorder = QColor("#ff3090");
      m_builtinPresets.append(t); }

    // 9. Ocean Deep
    { Theme t; t.name = "Ocean Deep";
      t.windowBg = QColor("#0d1117"); t.panelBg = QColor("#161b22");
      t.hudBg = QColor("#161b22"); t.hudBorder = QColor("#30363d");
      t.inputBg = QColor("#0d1117"); t.inputBorder = QColor("#30363d"); t.inputFocusBorder = QColor("#1f6feb");
      t.buttonBg = QColor("#21262d"); t.buttonHoverBg = QColor("#30363d"); t.buttonPressedBg = QColor("#1158c7");
      t.buttonBorder = QColor("#30363d"); t.buttonText = QColor("#c9d1d9");
      t.textPrimary = QColor("#c9d1d9"); t.textSecondary = QColor("#8b949e");
      t.treeBg = QColor("#0d1117"); t.treeAltBg = QColor("#121820"); t.treeItemHover = QColor("#161b22");
      t.treeItemSelected = QColor("#1f6feb"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(88, 166, 255, 90);
      t.headerBg = QColor("#161b22"); t.headerText = QColor("#c9d1d9"); t.headerBorder = QColor("#0d1117");
      t.progressBg = QColor("#161b22"); t.progressChunk = QColor("#58a6ff"); t.progressText = QColor("#c9d1d9"); t.progressBorder = QColor("#30363d");
      t.menuBg = QColor("#161b22"); t.menuText = QColor("#c9d1d9"); t.menuSelectedBg = QColor("#1f6feb"); t.menuBorder = QColor("#30363d");
      t.deleteEnabledBg = QColor("#da3633"); t.deleteEnabledBorder = QColor("#f85149");
      m_builtinPresets.append(t); }

    // 10. Sunset
    { Theme t; t.name = "Sunset";
      t.windowBg = QColor("#1a1423"); t.panelBg = QColor("#221a2e");
      t.hudBg = QColor("#221a2e"); t.hudBorder = QColor("#3a2a4a");
      t.inputBg = QColor("#2a1e38"); t.inputBorder = QColor("#3a2a4a"); t.inputFocusBorder = QColor("#ff6b35");
      t.buttonBg = QColor("#322440"); t.buttonHoverBg = QColor("#422e52"); t.buttonPressedBg = QColor("#cc5528");
      t.buttonBorder = QColor("#3a2a4a"); t.buttonText = QColor("#f0d8c0");
      t.textPrimary = QColor("#f0d8c0"); t.textSecondary = QColor("#8a6a6a");
      t.treeBg = QColor("#1a1423"); t.treeAltBg = QColor("#201828"); t.treeItemHover = QColor("#2a1e38");
      t.treeItemSelected = QColor("#ff6b35"); t.treeItemSelectedText = QColor("#1a1423");
      t.percentBarColor = QColor(255, 107, 53, 100);
      t.headerBg = QColor("#221a2e"); t.headerText = QColor("#e0c8b0"); t.headerBorder = QColor("#1a1423");
      t.progressBg = QColor("#221a2e"); t.progressChunk = QColor("#f7c948"); t.progressText = QColor("#f0d8c0"); t.progressBorder = QColor("#3a2a4a");
      t.menuBg = QColor("#221a2e"); t.menuText = QColor("#f0d8c0"); t.menuSelectedBg = QColor("#ff6b35"); t.menuBorder = QColor("#3a2a4a");
      t.deleteEnabledBg = QColor("#e53e3e"); t.deleteEnabledBorder = QColor("#fc8181");
      m_builtinPresets.append(t); }

    // 11. Matrix
    { Theme t; t.name = "Matrix";
      t.windowBg = QColor("#0a0a0a"); t.panelBg = QColor("#0d0d0d");
      t.hudBg = QColor("#0d0d0d"); t.hudBorder = QColor("#1a3a1a");
      t.inputBg = QColor("#0f0f0f"); t.inputBorder = QColor("#1a3a1a"); t.inputFocusBorder = QColor("#00ff41");
      t.buttonBg = QColor("#141f14"); t.buttonHoverBg = QColor("#1a2e1a"); t.buttonPressedBg = QColor("#008822");
      t.buttonBorder = QColor("#1a3a1a"); t.buttonText = QColor("#00ff41");
      t.textPrimary = QColor("#00ff41"); t.textSecondary = QColor("#008822");
      t.treeBg = QColor("#0a0a0a"); t.treeAltBg = QColor("#0d0f0d"); t.treeItemHover = QColor("#0f1a0f");
      t.treeItemSelected = QColor("#00ff41"); t.treeItemSelectedText = QColor("#0a0a0a");
      t.percentBarColor = QColor(0, 255, 65, 80);
      t.headerBg = QColor("#0f140f"); t.headerText = QColor("#00cc33"); t.headerBorder = QColor("#0a0a0a");
      t.progressBg = QColor("#0d0d0d"); t.progressChunk = QColor("#00ff41"); t.progressText = QColor("#00ff41"); t.progressBorder = QColor("#1a3a1a");
      t.menuBg = QColor("#0d0d0d"); t.menuText = QColor("#00ff41"); t.menuSelectedBg = QColor("#008822"); t.menuBorder = QColor("#1a3a1a");
      t.deleteEnabledBg = QColor("#aa0000"); t.deleteEnabledBorder = QColor("#ff0000");
      m_builtinPresets.append(t); }

    // 12. Rose Gold
    { Theme t; t.name = "Rose Gold";
      t.windowBg = QColor("#1e1a20"); t.panelBg = QColor("#261e28");
      t.hudBg = QColor("#261e28"); t.hudBorder = QColor("#3a2e3e");
      t.inputBg = QColor("#2a2230"); t.inputBorder = QColor("#3a2e3e"); t.inputFocusBorder = QColor("#d4a574");
      t.buttonBg = QColor("#332838"); t.buttonHoverBg = QColor("#443448"); t.buttonPressedBg = QColor("#a87d55");
      t.buttonBorder = QColor("#3a2e3e"); t.buttonText = QColor("#f0d8d0");
      t.textPrimary = QColor("#f0d8d0"); t.textSecondary = QColor("#8a6a78");
      t.treeBg = QColor("#1e1a20"); t.treeAltBg = QColor("#241e26"); t.treeItemHover = QColor("#2e262e");
      t.treeItemSelected = QColor("#d4a574"); t.treeItemSelectedText = QColor("#1e1a20");
      t.percentBarColor = QColor(232, 180, 184, 100);
      t.headerBg = QColor("#261e28"); t.headerText = QColor("#e0c8c0"); t.headerBorder = QColor("#1e1a20");
      t.progressBg = QColor("#261e28"); t.progressChunk = QColor("#e8b4b8"); t.progressText = QColor("#f0d8d0"); t.progressBorder = QColor("#3a2e3e");
      t.menuBg = QColor("#261e28"); t.menuText = QColor("#f0d8d0"); t.menuSelectedBg = QColor("#d4a574"); t.menuBorder = QColor("#3a2e3e");
      t.deleteEnabledBg = QColor("#cc3355"); t.deleteEnabledBorder = QColor("#ff5577");
      m_builtinPresets.append(t); }

    // 13. Arctic (LIGHT THEME)
    { Theme t; t.name = "Arctic";
      t.windowBg = QColor("#f5f7fa"); t.panelBg = QColor("#ebedf0");
      t.hudBg = QColor("#ebedf0"); t.hudBorder = QColor("#d1d5db");
      t.inputBg = QColor("#ffffff"); t.inputBorder = QColor("#d1d5db"); t.inputFocusBorder = QColor("#2563eb");
      t.buttonBg = QColor("#e5e7eb"); t.buttonHoverBg = QColor("#d1d5db"); t.buttonPressedBg = QColor("#1d4ed8");
      t.buttonBorder = QColor("#d1d5db"); t.buttonText = QColor("#1f2937");
      t.textPrimary = QColor("#1f2937"); t.textSecondary = QColor("#6b7280");
      t.treeBg = QColor("#ffffff"); t.treeAltBg = QColor("#f9fafb"); t.treeItemHover = QColor("#f3f4f6");
      t.treeItemSelected = QColor("#2563eb"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(37, 99, 235, 80);
      t.headerBg = QColor("#f3f4f6"); t.headerText = QColor("#374151"); t.headerBorder = QColor("#e5e7eb");
      t.progressBg = QColor("#e5e7eb"); t.progressChunk = QColor("#2563eb"); t.progressText = QColor("#1f2937"); t.progressBorder = QColor("#d1d5db");
      t.menuBg = QColor("#ffffff"); t.menuText = QColor("#1f2937"); t.menuSelectedBg = QColor("#2563eb"); t.menuBorder = QColor("#d1d5db");
      t.deleteEnabledBg = QColor("#ef4444"); t.deleteEnabledBorder = QColor("#dc2626");
      m_builtinPresets.append(t); }

    // 14. Emerald
    { Theme t; t.name = "Emerald";
      t.windowBg = QColor("#0f1a14"); t.panelBg = QColor("#14221a");
      t.hudBg = QColor("#14221a"); t.hudBorder = QColor("#1e3a28");
      t.inputBg = QColor("#182a1e"); t.inputBorder = QColor("#1e3a28"); t.inputFocusBorder = QColor("#10b981");
      t.buttonBg = QColor("#1e3228"); t.buttonHoverBg = QColor("#284238"); t.buttonPressedBg = QColor("#059669");
      t.buttonBorder = QColor("#1e3a28"); t.buttonText = QColor("#d0f0e0");
      t.textPrimary = QColor("#d0f0e0"); t.textSecondary = QColor("#5a8a6a");
      t.treeBg = QColor("#0f1a14"); t.treeAltBg = QColor("#121e18"); t.treeItemHover = QColor("#182a1e");
      t.treeItemSelected = QColor("#10b981"); t.treeItemSelectedText = QColor("#0f1a14");
      t.percentBarColor = QColor(52, 211, 153, 90);
      t.headerBg = QColor("#14221a"); t.headerText = QColor("#b0e0c8"); t.headerBorder = QColor("#0f1a14");
      t.progressBg = QColor("#14221a"); t.progressChunk = QColor("#34d399"); t.progressText = QColor("#d0f0e0"); t.progressBorder = QColor("#1e3a28");
      t.menuBg = QColor("#14221a"); t.menuText = QColor("#d0f0e0"); t.menuSelectedBg = QColor("#10b981"); t.menuBorder = QColor("#1e3a28");
      t.deleteEnabledBg = QColor("#dc2626"); t.deleteEnabledBorder = QColor("#f87171");
      m_builtinPresets.append(t); }

    // 15. Sakura
    { Theme t; t.name = "Sakura";
      t.windowBg = QColor("#1a1215"); t.panelBg = QColor("#22181c");
      t.hudBg = QColor("#22181c"); t.hudBorder = QColor("#3a2830");
      t.inputBg = QColor("#2a1e24"); t.inputBorder = QColor("#3a2830"); t.inputFocusBorder = QColor("#f472b6");
      t.buttonBg = QColor("#332430"); t.buttonHoverBg = QColor("#443040"); t.buttonPressedBg = QColor("#c05090");
      t.buttonBorder = QColor("#3a2830"); t.buttonText = QColor("#f8d0e8");
      t.textPrimary = QColor("#f8d0e8"); t.textSecondary = QColor("#8a5a78");
      t.treeBg = QColor("#1a1215"); t.treeAltBg = QColor("#1e161a"); t.treeItemHover = QColor("#2a1e24");
      t.treeItemSelected = QColor("#f472b6"); t.treeItemSelectedText = QColor("#1a1215");
      t.percentBarColor = QColor(251, 207, 232, 90);
      t.headerBg = QColor("#22181c"); t.headerText = QColor("#e8b0d0"); t.headerBorder = QColor("#1a1215");
      t.progressBg = QColor("#22181c"); t.progressChunk = QColor("#fbcfe8"); t.progressText = QColor("#f8d0e8"); t.progressBorder = QColor("#3a2830");
      t.menuBg = QColor("#22181c"); t.menuText = QColor("#f8d0e8"); t.menuSelectedBg = QColor("#f472b6"); t.menuBorder = QColor("#3a2830");
      t.deleteEnabledBg = QColor("#e11d48"); t.deleteEnabledBorder = QColor("#fb7185");
      m_builtinPresets.append(t); }

    // 16. Cobalt
    { Theme t; t.name = "Cobalt";
      t.windowBg = QColor("#0d1a2d"); t.panelBg = QColor("#122240");
      t.hudBg = QColor("#122240"); t.hudBorder = QColor("#1e3a5a");
      t.inputBg = QColor("#162a48"); t.inputBorder = QColor("#1e3a5a"); t.inputFocusBorder = QColor("#0088ff");
      t.buttonBg = QColor("#1a3050"); t.buttonHoverBg = QColor("#244060"); t.buttonPressedBg = QColor("#006dcc");
      t.buttonBorder = QColor("#1e3a5a"); t.buttonText = QColor("#c0e0ff");
      t.textPrimary = QColor("#c0e0ff"); t.textSecondary = QColor("#5a8ab0");
      t.treeBg = QColor("#0d1a2d"); t.treeAltBg = QColor("#101e34"); t.treeItemHover = QColor("#162a48");
      t.treeItemSelected = QColor("#0088ff"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(79, 195, 247, 90);
      t.headerBg = QColor("#122240"); t.headerText = QColor("#a0d0f0"); t.headerBorder = QColor("#0d1a2d");
      t.progressBg = QColor("#122240"); t.progressChunk = QColor("#4fc3f7"); t.progressText = QColor("#c0e0ff"); t.progressBorder = QColor("#1e3a5a");
      t.menuBg = QColor("#122240"); t.menuText = QColor("#c0e0ff"); t.menuSelectedBg = QColor("#0088ff"); t.menuBorder = QColor("#1e3a5a");
      t.deleteEnabledBg = QColor("#e53935"); t.deleteEnabledBorder = QColor("#ef5350");
      m_builtinPresets.append(t); }

    // 17. Lavender
    { Theme t; t.name = "Lavender";
      t.windowBg = QColor("#1a1625"); t.panelBg = QColor("#221e30");
      t.hudBg = QColor("#221e30"); t.hudBorder = QColor("#3a2e50");
      t.inputBg = QColor("#2a2438"); t.inputBorder = QColor("#3a2e50"); t.inputFocusBorder = QColor("#a78bfa");
      t.buttonBg = QColor("#322a44"); t.buttonHoverBg = QColor("#423658"); t.buttonPressedBg = QColor("#7c5cbf");
      t.buttonBorder = QColor("#3a2e50"); t.buttonText = QColor("#e0d0f8");
      t.textPrimary = QColor("#e0d0f8"); t.textSecondary = QColor("#7a6aa0");
      t.treeBg = QColor("#1a1625"); t.treeAltBg = QColor("#1e1a2a"); t.treeItemHover = QColor("#2a2438");
      t.treeItemSelected = QColor("#a78bfa"); t.treeItemSelectedText = QColor("#1a1625");
      t.percentBarColor = QColor(196, 181, 253, 90);
      t.headerBg = QColor("#221e30"); t.headerText = QColor("#c8b8e8"); t.headerBorder = QColor("#1a1625");
      t.progressBg = QColor("#221e30"); t.progressChunk = QColor("#c4b5fd"); t.progressText = QColor("#e0d0f8"); t.progressBorder = QColor("#3a2e50");
      t.menuBg = QColor("#221e30"); t.menuText = QColor("#e0d0f8"); t.menuSelectedBg = QColor("#a78bfa"); t.menuBorder = QColor("#3a2e50");
      t.deleteEnabledBg = QColor("#dc2626"); t.deleteEnabledBorder = QColor("#f87171");
      m_builtinPresets.append(t); }

    // 18. Copper
    { Theme t; t.name = "Copper";
      t.windowBg = QColor("#1a1410"); t.panelBg = QColor("#221c14");
      t.hudBg = QColor("#221c14"); t.hudBorder = QColor("#3a2e1e");
      t.inputBg = QColor("#2a221a"); t.inputBorder = QColor("#3a2e1e"); t.inputFocusBorder = QColor("#d97706");
      t.buttonBg = QColor("#332a1e"); t.buttonHoverBg = QColor("#443828"); t.buttonPressedBg = QColor("#b45d05");
      t.buttonBorder = QColor("#3a2e1e"); t.buttonText = QColor("#f0dcc0");
      t.textPrimary = QColor("#f0dcc0"); t.textSecondary = QColor("#8a7050");
      t.treeBg = QColor("#1a1410"); t.treeAltBg = QColor("#1e1814"); t.treeItemHover = QColor("#2a221a");
      t.treeItemSelected = QColor("#d97706"); t.treeItemSelectedText = QColor("#1a1410");
      t.percentBarColor = QColor(245, 158, 11, 90);
      t.headerBg = QColor("#221c14"); t.headerText = QColor("#e0c8a0"); t.headerBorder = QColor("#1a1410");
      t.progressBg = QColor("#221c14"); t.progressChunk = QColor("#f59e0b"); t.progressText = QColor("#f0dcc0"); t.progressBorder = QColor("#3a2e1e");
      t.menuBg = QColor("#221c14"); t.menuText = QColor("#f0dcc0"); t.menuSelectedBg = QColor("#d97706"); t.menuBorder = QColor("#3a2e1e");
      t.deleteEnabledBg = QColor("#dc2626"); t.deleteEnabledBorder = QColor("#f87171");
      m_builtinPresets.append(t); }

    // 19. Slate
    { Theme t; t.name = "Slate";
      t.windowBg = QColor("#1e293b"); t.panelBg = QColor("#0f172a");
      t.hudBg = QColor("#0f172a"); t.hudBorder = QColor("#334155");
      t.inputBg = QColor("#1e293b"); t.inputBorder = QColor("#334155"); t.inputFocusBorder = QColor("#94a3b8");
      t.buttonBg = QColor("#334155"); t.buttonHoverBg = QColor("#475569"); t.buttonPressedBg = QColor("#64748b");
      t.buttonBorder = QColor("#475569"); t.buttonText = QColor("#e2e8f0");
      t.textPrimary = QColor("#e2e8f0"); t.textSecondary = QColor("#94a3b8");
      t.treeBg = QColor("#1e293b"); t.treeAltBg = QColor("#222f42"); t.treeItemHover = QColor("#2a3a50");
      t.treeItemSelected = QColor("#94a3b8"); t.treeItemSelectedText = QColor("#0f172a");
      t.percentBarColor = QColor(203, 213, 225, 90);
      t.headerBg = QColor("#0f172a"); t.headerText = QColor("#cbd5e1"); t.headerBorder = QColor("#1e293b");
      t.progressBg = QColor("#0f172a"); t.progressChunk = QColor("#cbd5e1"); t.progressText = QColor("#e2e8f0"); t.progressBorder = QColor("#334155");
      t.menuBg = QColor("#0f172a"); t.menuText = QColor("#e2e8f0"); t.menuSelectedBg = QColor("#94a3b8"); t.menuBorder = QColor("#334155");
      t.deleteEnabledBg = QColor("#ef4444"); t.deleteEnabledBorder = QColor("#f87171");
      m_builtinPresets.append(t); }

    // 20. Neon Tokyo
    { Theme t; t.name = "Neon Tokyo";
      t.windowBg = QColor("#050505"); t.panelBg = QColor("#0a0a0a");
      t.hudBg = QColor("#0a0a0a"); t.hudBorder = QColor("#1a1a1a");
      t.inputBg = QColor("#0f0f0f"); t.inputBorder = QColor("#1a1a1a"); t.inputFocusBorder = QColor("#ff3864");
      t.buttonBg = QColor("#141414"); t.buttonHoverBg = QColor("#1e1e1e"); t.buttonPressedBg = QColor("#cc2e50");
      t.buttonBorder = QColor("#2a1a2a"); t.buttonText = QColor("#f0e0f0");
      t.textPrimary = QColor("#f0e0f0"); t.textSecondary = QColor("#6a4a6a");
      t.treeBg = QColor("#050505"); t.treeAltBg = QColor("#0a0a0a"); t.treeItemHover = QColor("#141414");
      t.treeItemSelected = QColor("#ff3864"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(255, 56, 100, 80);
      t.headerBg = QColor("#0a0a0a"); t.headerText = QColor("#e0c0e0"); t.headerBorder = QColor("#050505");
      t.progressBg = QColor("#0a0a0a"); t.progressChunk = QColor("#2de2e6"); t.progressText = QColor("#f0e0f0"); t.progressBorder = QColor("#1a1a1a");
      t.menuBg = QColor("#0a0a0a"); t.menuText = QColor("#f0e0f0"); t.menuSelectedBg = QColor("#ff3864"); t.menuBorder = QColor("#1a1a1a");
      t.deleteEnabledBg = QColor("#ff0040"); t.deleteEnabledBorder = QColor("#ff3070");
      m_builtinPresets.append(t); }

    // 21. Retro Hacker (Old-school CRT phosphor green on pitch black)
    { Theme t; t.name = "Retro Hacker";
      t.windowBg = QColor("#000500"); t.panelBg = QColor("#000c00");
      t.hudBg = QColor("#000c00"); t.hudBorder = QColor("#003300");
      t.inputBg = QColor("#001100"); t.inputBorder = QColor("#004400"); t.inputFocusBorder = QColor("#00ff66");
      t.buttonBg = QColor("#002200"); t.buttonHoverBg = QColor("#003300"); t.buttonPressedBg = QColor("#006611");
      t.buttonBorder = QColor("#005500"); t.buttonText = QColor("#33ff33");
      t.textPrimary = QColor("#33ff33"); t.textSecondary = QColor("#009900");
      t.treeBg = QColor("#000500"); t.treeAltBg = QColor("#000a00"); t.treeItemHover = QColor("#001a00");
      t.treeItemSelected = QColor("#33ff33"); t.treeItemSelectedText = QColor("#000000");
      t.percentBarColor = QColor(51, 255, 51, 100);
      t.headerBg = QColor("#001100"); t.headerText = QColor("#00ff66"); t.headerBorder = QColor("#002200");
      t.progressBg = QColor("#000f00"); t.progressChunk = QColor("#33ff33"); t.progressText = QColor("#33ff33"); t.progressBorder = QColor("#004400");
      t.menuBg = QColor("#000c00"); t.menuText = QColor("#33ff33"); t.menuSelectedBg = QColor("#006600"); t.menuBorder = QColor("#004400");
      t.deleteEnabledBg = QColor("#cc0000"); t.deleteEnabledBorder = QColor("#ff3333");
      m_builtinPresets.append(t); }

    // 22. Cyber Yellow (Vibrant high contrast yellow & black)
    { Theme t; t.name = "Cyber Yellow";
      t.windowBg = QColor("#121100"); t.panelBg = QColor("#1c1a00");
      t.hudBg = QColor("#1c1a00"); t.hudBorder = QColor("#444000");
      t.inputBg = QColor("#262300"); t.inputBorder = QColor("#555000"); t.inputFocusBorder = QColor("#ffee00");
      t.buttonBg = QColor("#333000"); t.buttonHoverBg = QColor("#4b4700"); t.buttonPressedBg = QColor("#998c00");
      t.buttonBorder = QColor("#ffee00"); t.buttonText = QColor("#ffee00");
      t.textPrimary = QColor("#ffffaa"); t.textSecondary = QColor("#bbaa00");
      t.treeBg = QColor("#121100"); t.treeAltBg = QColor("#181600"); t.treeItemHover = QColor("#2b2800");
      t.treeItemSelected = QColor("#ffee00"); t.treeItemSelectedText = QColor("#000000");
      t.percentBarColor = QColor(255, 238, 0, 110);
      t.headerBg = QColor("#222000"); t.headerText = QColor("#ffee00"); t.headerBorder = QColor("#444000");
      t.progressBg = QColor("#1c1a00"); t.progressChunk = QColor("#ffee00"); t.progressText = QColor("#121100"); t.progressBorder = QColor("#ffee00");
      t.menuBg = QColor("#1c1a00"); t.menuText = QColor("#ffffaa"); t.menuSelectedBg = QColor("#ffee00"); t.menuBorder = QColor("#555000");
      t.deleteEnabledBg = QColor("#ff2200"); t.deleteEnabledBorder = QColor("#ff6600");
      m_builtinPresets.append(t); }

    // 23. Crimson Vibe (Deep glowing crimson red)
    { Theme t; t.name = "Crimson Vibe";
      t.windowBg = QColor("#1a0505"); t.panelBg = QColor("#260808");
      t.hudBg = QColor("#260808"); t.hudBorder = QColor("#4d1010");
      t.inputBg = QColor("#330b0b"); t.inputBorder = QColor("#5e1414"); t.inputFocusBorder = QColor("#ff2233");
      t.buttonBg = QColor("#400d0d"); t.buttonHoverBg = QColor("#541212"); t.buttonPressedBg = QColor("#991414");
      t.buttonBorder = QColor("#7a1a1a"); t.buttonText = QColor("#ffcccc");
      t.textPrimary = QColor("#ffe6e6"); t.textSecondary = QColor("#b35959");
      t.treeBg = QColor("#1a0505"); t.treeAltBg = QColor("#210606"); t.treeItemHover = QColor("#380c0c");
      t.treeItemSelected = QColor("#ff2233"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(255, 34, 51, 100);
      t.headerBg = QColor("#2e0a0a"); t.headerText = QColor("#ff9999"); t.headerBorder = QColor("#4d1010");
      t.progressBg = QColor("#260808"); t.progressChunk = QColor("#ff3344"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#ff2233");
      t.menuBg = QColor("#260808"); t.menuText = QColor("#ffe6e6"); t.menuSelectedBg = QColor("#ff2233"); t.menuBorder = QColor("#5e1414");
      t.deleteEnabledBg = QColor("#ff0022"); t.deleteEnabledBorder = QColor("#ff5566");
      m_builtinPresets.append(t); }

    // 24. Forest Ranger (Deep pine & lime green)
    { Theme t; t.name = "Forest Ranger";
      t.windowBg = QColor("#091409"); t.panelBg = QColor("#0f210f");
      t.hudBg = QColor("#0f210f"); t.hudBorder = QColor("#1d401d");
      t.inputBg = QColor("#142b14"); t.inputBorder = QColor("#255225"); t.inputFocusBorder = QColor("#76e034");
      t.buttonBg = QColor("#1a381a"); t.buttonHoverBg = QColor("#234a23"); t.buttonPressedBg = QColor("#3f801d");
      t.buttonBorder = QColor("#2e612e"); t.buttonText = QColor("#e2f7d5");
      t.textPrimary = QColor("#e2f7d5"); t.textSecondary = QColor("#7eb370");
      t.treeBg = QColor("#091409"); t.treeAltBg = QColor("#0c1b0c"); t.treeItemHover = QColor("#173317");
      t.treeItemSelected = QColor("#76e034"); t.treeItemSelectedText = QColor("#091409");
      t.percentBarColor = QColor(118, 224, 52, 100);
      t.headerBg = QColor("#122912"); t.headerText = QColor("#adf08d"); t.headerBorder = QColor("#1d401d");
      t.progressBg = QColor("#0f210f"); t.progressChunk = QColor("#76e034"); t.progressText = QColor("#091409"); t.progressBorder = QColor("#255225");
      t.menuBg = QColor("#0f210f"); t.menuText = QColor("#e2f7d5"); t.menuSelectedBg = QColor("#76e034"); t.menuBorder = QColor("#255225");
      t.deleteEnabledBg = QColor("#d62828"); t.deleteEnabledBorder = QColor("#f77f00");
      m_builtinPresets.append(t); }

    // 25. Golden Hour (Warm amber, gold & honey)
    { Theme t; t.name = "Golden Hour";
      t.windowBg = QColor("#191206"); t.panelBg = QColor("#261c09");
      t.hudBg = QColor("#261c09"); t.hudBorder = QColor("#4d3813");
      t.inputBg = QColor("#33250c"); t.inputBorder = QColor("#5e4417"); t.inputFocusBorder = QColor("#ffb703");
      t.buttonBg = QColor("#402f0f"); t.buttonHoverBg = QColor("#543e14"); t.buttonPressedBg = QColor("#cc9202");
      t.buttonBorder = QColor("#7a5b1d"); t.buttonText = QColor("#ffeed4");
      t.textPrimary = QColor("#fff4e0"); t.textSecondary = QColor("#c7a26b");
      t.treeBg = QColor("#191206"); t.treeAltBg = QColor("#201708"); t.treeItemHover = QColor("#38290e");
      t.treeItemSelected = QColor("#ffb703"); t.treeItemSelectedText = QColor("#191206");
      t.percentBarColor = QColor(255, 183, 3, 100);
      t.headerBg = QColor("#2e220b"); t.headerText = QColor("#ffd166"); t.headerBorder = QColor("#4d3813");
      t.progressBg = QColor("#261c09"); t.progressChunk = QColor("#ffb703"); t.progressText = QColor("#191206"); t.progressBorder = QColor("#5e4417");
      t.menuBg = QColor("#261c09"); t.menuText = QColor("#fff4e0"); t.menuSelectedBg = QColor("#ffb703"); t.menuBorder = QColor("#5e4417");
      t.deleteEnabledBg = QColor("#d62828"); t.deleteEnabledBorder = QColor("#f77f00");
      m_builtinPresets.append(t); }

    // 26. Synthwave 84 (Purple neon & hot magenta)
    { Theme t; t.name = "Synthwave 84";
      t.windowBg = QColor("#171520"); t.panelBg = QColor("#241b2f");
      t.hudBg = QColor("#241b2f"); t.hudBorder = QColor("#442a58");
      t.inputBg = QColor("#2a1d3d"); t.inputBorder = QColor("#5a3378"); t.inputFocusBorder = QColor("#ff007f");
      t.buttonBg = QColor("#371f4b"); t.buttonHoverBg = QColor("#4a2965"); t.buttonPressedBg = QColor("#b30059");
      t.buttonBorder = QColor("#6d3594"); t.buttonText = QColor("#ffe6f2");
      t.textPrimary = QColor("#ffe6f2"); t.textSecondary = QColor("#aa66cc");
      t.treeBg = QColor("#171520"); t.treeAltBg = QColor("#1f192b"); t.treeItemHover = QColor("#312047");
      t.treeItemSelected = QColor("#ff007f"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(255, 0, 127, 100);
      t.headerBg = QColor("#2b1b3b"); t.headerText = QColor("#36f9f6"); t.headerBorder = QColor("#442a58");
      t.progressBg = QColor("#241b2f"); t.progressChunk = QColor("#36f9f6"); t.progressText = QColor("#171520"); t.progressBorder = QColor("#ff007f");
      t.menuBg = QColor("#241b2f"); t.menuText = QColor("#ffe6f2"); t.menuSelectedBg = QColor("#ff007f"); t.menuBorder = QColor("#5a3378");
      t.deleteEnabledBg = QColor("#fe0000"); t.deleteEnabledBorder = QColor("#ff5555");
      m_builtinPresets.append(t); }

    // 27. Electric Cyan (High tech glowing blue-cyan)
    { Theme t; t.name = "Electric Cyan";
      t.windowBg = QColor("#04131a"); t.panelBg = QColor("#081d28");
      t.hudBg = QColor("#081d28"); t.hudBorder = QColor("#11394e");
      t.inputBg = QColor("#0b2635"); t.inputBorder = QColor("#164a66"); t.inputFocusBorder = QColor("#00e5ff");
      t.buttonBg = QColor("#0e3247"); t.buttonHoverBg = QColor("#144461"); t.buttonPressedBg = QColor("#0099ab");
      t.buttonBorder = QColor("#1c597d"); t.buttonText = QColor("#d6f9ff");
      t.textPrimary = QColor("#e6fcff"); t.textSecondary = QColor("#5caec0");
      t.treeBg = QColor("#04131a"); t.treeAltBg = QColor("#061821"); t.treeItemHover = QColor("#0e2e3d");
      t.treeItemSelected = QColor("#00e5ff"); t.treeItemSelectedText = QColor("#04131a");
      t.percentBarColor = QColor(0, 229, 255, 100);
      t.headerBg = QColor("#092230"); t.headerText = QColor("#80f3ff"); t.headerBorder = QColor("#11394e");
      t.progressBg = QColor("#081d28"); t.progressChunk = QColor("#00e5ff"); t.progressText = QColor("#04131a"); t.progressBorder = QColor("#164a66");
      t.menuBg = QColor("#081d28"); t.menuText = QColor("#e6fcff"); t.menuSelectedBg = QColor("#00e5ff"); t.menuBorder = QColor("#164a66");
      t.deleteEnabledBg = QColor("#ff1744"); t.deleteEnabledBorder = QColor("#ff616f");
      m_builtinPresets.append(t); }

    // 28. Toxic Slime (Neon lime green on dark purple)
    { Theme t; t.name = "Toxic Slime";
      t.windowBg = QColor("#10081a"); t.panelBg = QColor("#1a0d2b");
      t.hudBg = QColor("#1a0d2b"); t.hudBorder = QColor("#331954");
      t.inputBg = QColor("#221138"); t.inputBorder = QColor("#442270"); t.inputFocusBorder = QColor("#39ff14");
      t.buttonBg = QColor("#2c1647"); t.buttonHoverBg = QColor("#3a1d5e"); t.buttonPressedBg = QColor("#22990c");
      t.buttonBorder = QColor("#552b8c"); t.buttonText = QColor("#e6ffe6");
      t.textPrimary = QColor("#f0fff0"); t.textSecondary = QColor("#88cc77");
      t.treeBg = QColor("#10081a"); t.treeAltBg = QColor("#150b22"); t.treeItemHover = QColor("#26133d");
      t.treeItemSelected = QColor("#39ff14"); t.treeItemSelectedText = QColor("#10081a");
      t.percentBarColor = QColor(57, 255, 20, 100);
      t.headerBg = QColor("#1f0f33"); t.headerText = QColor("#a3ff85"); t.headerBorder = QColor("#331954");
      t.progressBg = QColor("#1a0d2b"); t.progressChunk = QColor("#39ff14"); t.progressText = QColor("#10081a"); t.progressBorder = QColor("#442270");
      t.menuBg = QColor("#1a0d2b"); t.menuText = QColor("#f0fff0"); t.menuSelectedBg = QColor("#39ff14"); t.menuBorder = QColor("#442270");
      t.deleteEnabledBg = QColor("#ff0055"); t.deleteEnabledBorder = QColor("#ff4488");
      m_builtinPresets.append(t); }

    // 29. Sunset Blaze (Fiery orange, pink & magenta)
    { Theme t; t.name = "Sunset Blaze";
      t.windowBg = QColor("#1c0a14"); t.panelBg = QColor("#2b0f1f");
      t.hudBg = QColor("#2b0f1f"); t.hudBorder = QColor("#541e3d");
      t.inputBg = QColor("#381429"); t.inputBorder = QColor("#6e2750"); t.inputFocusBorder = QColor("#ff5e00");
      t.buttonBg = QColor("#471a34"); t.buttonHoverBg = QColor("#592041"); t.buttonPressedBg = QColor("#cc4b00");
      t.buttonBorder = QColor("#853062"); t.buttonText = QColor("#ffe6f0");
      t.textPrimary = QColor("#fff0f5"); t.textSecondary = QColor("#cc7799");
      t.treeBg = QColor("#1c0a14"); t.treeAltBg = QColor("#240d1a"); t.treeItemHover = QColor("#3d162d");
      t.treeItemSelected = QColor("#ff5e00"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(255, 94, 0, 100);
      t.headerBg = QColor("#301123"); t.headerText = QColor("#ff99c8"); t.headerBorder = QColor("#541e3d");
      t.progressBg = QColor("#2b0f1f"); t.progressChunk = QColor("#ff0077"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#ff5e00");
      t.menuBg = QColor("#2b0f1f"); t.menuText = QColor("#fff0f5"); t.menuSelectedBg = QColor("#ff5e00"); t.menuBorder = QColor("#6e2750");
      t.deleteEnabledBg = QColor("#e60000"); t.deleteEnabledBorder = QColor("#ff4444");
      m_builtinPresets.append(t); }

    // 30. Banana Split (Vibrant pastel yellow & chocolate)
    { Theme t; t.name = "Banana Split";
      t.windowBg = QColor("#241c14"); t.panelBg = QColor("#33271c");
      t.hudBg = QColor("#33271c"); t.hudBorder = QColor("#5e4934");
      t.inputBg = QColor("#403123"); t.inputBorder = QColor("#7a5f43"); t.inputFocusBorder = QColor("#fff066");
      t.buttonBg = QColor("#4f3d2b"); t.buttonHoverBg = QColor("#614b35"); t.buttonPressedBg = QColor("#cca300");
      t.buttonBorder = QColor("#967452"); t.buttonText = QColor("#fff9cc");
      t.textPrimary = QColor("#fffde6"); t.textSecondary = QColor("#ccb888");
      t.treeBg = QColor("#241c14"); t.treeAltBg = QColor("#2b2118"); t.treeItemHover = QColor("#453526");
      t.treeItemSelected = QColor("#fff066"); t.treeItemSelectedText = QColor("#241c14");
      t.percentBarColor = QColor(255, 240, 102, 100);
      t.headerBg = QColor("#3b2d20"); t.headerText = QColor("#ffe680"); t.headerBorder = QColor("#5e4934");
      t.progressBg = QColor("#33271c"); t.progressChunk = QColor("#fff066"); t.progressText = QColor("#241c14"); t.progressBorder = QColor("#7a5f43");
      t.menuBg = QColor("#33271c"); t.menuText = QColor("#fffde6"); t.menuSelectedBg = QColor("#fff066"); t.menuBorder = QColor("#7a5f43");
      t.deleteEnabledBg = QColor("#cc2200"); t.deleteEnabledBorder = QColor("#ff5533");
      m_builtinPresets.append(t); }

    // 31. Vampire Red (Dark blood red & onyx)
    { Theme t; t.name = "Vampire Red";
      t.windowBg = QColor("#0d0002"); t.panelBg = QColor("#170004");
      t.hudBg = QColor("#170004"); t.hudBorder = QColor("#38000a");
      t.inputBg = QColor("#210006"); t.inputBorder = QColor("#4a000e"); t.inputFocusBorder = QColor("#e60026");
      t.buttonBg = QColor("#2b0007"); t.buttonHoverBg = QColor("#3d000b"); t.buttonPressedBg = QColor("#99001a");
      t.buttonBorder = QColor("#660013"); t.buttonText = QColor("#ffccd3");
      t.textPrimary = QColor("#ffe6ea"); t.textSecondary = QColor("#b35968");
      t.treeBg = QColor("#0d0002"); t.treeAltBg = QColor("#120003"); t.treeItemHover = QColor("#240007");
      t.treeItemSelected = QColor("#e60026"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(230, 0, 38, 100);
      t.headerBg = QColor("#1c0005"); t.headerText = QColor("#ff8093"); t.headerBorder = QColor("#38000a");
      t.progressBg = QColor("#170004"); t.progressChunk = QColor("#e60026"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#4a000e");
      t.menuBg = QColor("#170004"); t.menuText = QColor("#ffe6ea"); t.menuSelectedBg = QColor("#e60026"); t.menuBorder = QColor("#4a000e");
      t.deleteEnabledBg = QColor("#ff0000"); t.deleteEnabledBorder = QColor("#ff4444");
      m_builtinPresets.append(t); }

    // 32. Mint Breeze (Fresh cool mint & teal)
    { Theme t; t.name = "Mint Breeze";
      t.windowBg = QColor("#061715"); t.panelBg = QColor("#0a2420");
      t.hudBg = QColor("#0a2420"); t.hudBorder = QColor("#13453d");
      t.inputBg = QColor("#0e302b"); t.inputBorder = QColor("#1a5c51"); t.inputFocusBorder = QColor("#00f5c4");
      t.buttonBg = QColor("#123d37"); t.buttonHoverBg = QColor("#184f47"); t.buttonPressedBg = QColor("#00a383");
      t.buttonBorder = QColor("#227a6c"); t.buttonText = QColor("#d6fff7");
      t.textPrimary = QColor("#e6fffb"); t.textSecondary = QColor("#66cca3");
      t.treeBg = QColor("#061715"); t.treeAltBg = QColor("#081f1c"); t.treeItemHover = QColor("#0f3832");
      t.treeItemSelected = QColor("#00f5c4"); t.treeItemSelectedText = QColor("#061715");
      t.percentBarColor = QColor(0, 245, 196, 100);
      t.headerBg = QColor("#0c2b26"); t.headerText = QColor("#80ffea"); t.headerBorder = QColor("#13453d");
      t.progressBg = QColor("#0a2420"); t.progressChunk = QColor("#00f5c4"); t.progressText = QColor("#061715"); t.progressBorder = QColor("#1a5c51");
      t.menuBg = QColor("#0a2420"); t.menuText = QColor("#e6fffb"); t.menuSelectedBg = QColor("#00f5c4"); t.menuBorder = QColor("#1a5c51");
      t.deleteEnabledBg = QColor("#ff3355"); t.deleteEnabledBorder = QColor("#ff6688");
      m_builtinPresets.append(t); }

    // 33. Lemonade Stand (Bright sunny yellow & sky blue)
    { Theme t; t.name = "Lemonade Stand";
      t.windowBg = QColor("#1a180a"); t.panelBg = QColor("#26230f");
      t.hudBg = QColor("#26230f"); t.hudBorder = QColor("#4a451e");
      t.inputBg = QColor("#332f14"); t.inputBorder = QColor("#615a27"); t.inputFocusBorder = QColor("#ffd700");
      t.buttonBg = QColor("#403b1a"); t.buttonHoverBg = QColor("#524b21"); t.buttonPressedBg = QColor("#b39700");
      t.buttonBorder = QColor("#786f30"); t.buttonText = QColor("#fff8cc");
      t.textPrimary = QColor("#fffde6"); t.textSecondary = QColor("#c2b866");
      t.treeBg = QColor("#1a180a"); t.treeAltBg = QColor("#211f0d"); t.treeItemHover = QColor("#363214");
      t.treeItemSelected = QColor("#00bfff"); t.treeItemSelectedText = QColor("#000000");
      t.percentBarColor = QColor(255, 215, 0, 100);
      t.headerBg = QColor("#2c2811"); t.headerText = QColor("#ffe680"); t.headerBorder = QColor("#4a4a1e");
      t.progressBg = QColor("#26230f"); t.progressChunk = QColor("#ffd700"); t.progressText = QColor("#1a180a"); t.progressBorder = QColor("#615a27");
      t.menuBg = QColor("#26230f"); t.menuText = QColor("#fffde6"); t.menuSelectedBg = QColor("#ffd700"); t.menuBorder = QColor("#615a27");
      t.deleteEnabledBg = QColor("#ff3300"); t.deleteEnabledBorder = QColor("#ff6633");
      m_builtinPresets.append(t); }

    // 34. Ruby Glow (Deep vibrant ruby gemstone red)
    { Theme t; t.name = "Ruby Glow";
      t.windowBg = QColor("#140408"); t.panelBg = QColor("#21060d");
      t.hudBg = QColor("#21060d"); t.hudBorder = QColor("#420c1a");
      t.inputBg = QColor("#2e0814"); t.inputBorder = QColor("#571022"); t.inputFocusBorder = QColor("#ff0044");
      t.buttonBg = QColor("#3b0b1a"); t.buttonHoverBg = QColor("#4c0e22"); t.buttonPressedBg = QColor("#990029");
      t.buttonBorder = QColor("#6e142b"); t.buttonText = QColor("#ffe0e8");
      t.textPrimary = QColor("#fff0f3"); t.textSecondary = QColor("#cc6680");
      t.treeBg = QColor("#140408"); t.treeAltBg = QColor("#1a050a"); t.treeItemHover = QColor("#330916");
      t.treeItemSelected = QColor("#ff0044"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(255, 0, 68, 100);
      t.headerBg = QColor("#290710"); t.headerText = QColor("#ff80a0"); t.headerBorder = QColor("#420c1a");
      t.progressBg = QColor("#21060d"); t.progressChunk = QColor("#ff0044"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#571022");
      t.menuBg = QColor("#21060d"); t.menuText = QColor("#fff0f3"); t.menuSelectedBg = QColor("#ff0044"); t.menuBorder = QColor("#571022");
      t.deleteEnabledBg = QColor("#d60000"); t.deleteEnabledBorder = QColor("#ff3333");
      m_builtinPresets.append(t); }

    // 35. Radioactive Green (Glowing neon green warning)
    { Theme t; t.name = "Radioactive Green";
      t.windowBg = QColor("#081204"); t.panelBg = QColor("#0e1e07");
      t.hudBg = QColor("#0e1e07"); t.hudBorder = QColor("#1c3d0e");
      t.inputBg = QColor("#13290a"); t.inputBorder = QColor("#255212"); t.inputFocusBorder = QColor("#66ff00");
      t.buttonBg = QColor("#18360c"); t.buttonHoverBg = QColor("#204710"); t.buttonPressedBg = QColor("#3da800");
      t.buttonBorder = QColor("#316b17"); t.buttonText = QColor("#e0ffcc");
      t.textPrimary = QColor("#f0ffe6"); t.textSecondary = QColor("#88cc55");
      t.treeBg = QColor("#081204"); t.treeAltBg = QColor("#0b1805"); t.treeItemHover = QColor("#153009");
      t.treeItemSelected = QColor("#66ff00"); t.treeItemSelectedText = QColor("#081204");
      t.percentBarColor = QColor(102, 255, 0, 100);
      t.headerBg = QColor("#112408"); t.headerText = QColor("#99ff33"); t.headerBorder = QColor("#1c3d0e");
      t.progressBg = QColor("#0e1e07"); t.progressChunk = QColor("#66ff00"); t.progressText = QColor("#081204"); t.progressBorder = QColor("#255212");
      t.menuBg = QColor("#0e1e07"); t.menuText = QColor("#f0ffe6"); t.menuSelectedBg = QColor("#66ff00"); t.menuBorder = QColor("#255212");
      t.deleteEnabledBg = QColor("#ff1100"); t.deleteEnabledBorder = QColor("#ff5500");
      m_builtinPresets.append(t); }

    // 36. Mustard Gold (Rich warm mustard yellow & charcoal)
    { Theme t; t.name = "Mustard Gold";
      t.windowBg = QColor("#1c1810"); t.panelBg = QColor("#292317");
      t.hudBg = QColor("#292317"); t.hudBorder = QColor("#4a3f29");
      t.inputBg = QColor("#362e1e"); t.inputBorder = QColor("#5e5035"); t.inputFocusBorder = QColor("#e6b800");
      t.buttonBg = QColor("#423925"); t.buttonHoverBg = QColor("#54482f"); t.buttonPressedBg = QColor("#a88700");
      t.buttonBorder = QColor("#756441"); t.buttonText = QColor("#fff3cc");
      t.textPrimary = QColor("#fff8e6"); t.textSecondary = QColor("#c2b085");
      t.treeBg = QColor("#1c1810"); t.treeAltBg = QColor("#241f14"); t.treeItemHover = QColor("#3b3221");
      t.treeItemSelected = QColor("#e6b800"); t.treeItemSelectedText = QColor("#1c1810");
      t.percentBarColor = QColor(230, 184, 0, 100);
      t.headerBg = QColor("#30291b"); t.headerText = QColor("#ffd633"); t.headerBorder = QColor("#4a3f29");
      t.progressBg = QColor("#292317"); t.progressChunk = QColor("#e6b800"); t.progressText = QColor("#1c1810"); t.progressBorder = QColor("#5e5035");
      t.menuBg = QColor("#292317"); t.menuText = QColor("#fff8e6"); t.menuSelectedBg = QColor("#e6b800"); t.menuBorder = QColor("#5e5035");
      t.deleteEnabledBg = QColor("#d63030"); t.deleteEnabledBorder = QColor("#ff5555");
      m_builtinPresets.append(t); }

    // 37. Blood Orange (Searing hot orange-red)
    { Theme t; t.name = "Blood Orange";
      t.windowBg = QColor("#1c0b05"); t.panelBg = QColor("#2b1008");
      t.hudBg = QColor("#2b1008"); t.hudBorder = QColor("#54200f");
      t.inputBg = QColor("#38150a"); t.inputBorder = QColor("#6b2813"); t.inputFocusBorder = QColor("#ff4500");
      t.buttonBg = QColor("#451a0d"); t.buttonHoverBg = QColor("#592211"); t.buttonPressedBg = QColor("#cc3700");
      t.buttonBorder = QColor("#82311c"); t.buttonText = QColor("#ffe4d6");
      t.textPrimary = QColor("#fff0e6"); t.textSecondary = QColor("#cc8570");
      t.treeBg = QColor("#1c0b05"); t.treeAltBg = QColor("#240e06"); t.treeItemHover = QColor("#3d170b");
      t.treeItemSelected = QColor("#ff4500"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(255, 69, 0, 100);
      t.headerBg = QColor("#301209"); t.headerText = QColor("#ff8c66"); t.headerBorder = QColor("#54200f");
      t.progressBg = QColor("#2b1008"); t.progressChunk = QColor("#ff4500"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#6b2813");
      t.menuBg = QColor("#2b1008"); t.menuText = QColor("#fff0e6"); t.menuSelectedBg = QColor("#ff4500"); t.menuBorder = QColor("#6b2813");
      t.deleteEnabledBg = QColor("#b30000"); t.deleteEnabledBorder = QColor("#ff3333");
      m_builtinPresets.append(t); }

    // 38. Lime Cyberpunk (Vivid green & purple neon)
    { Theme t; t.name = "Lime Cyberpunk";
      t.windowBg = QColor("#0d0514"); t.panelBg = QColor("#170924");
      t.hudBg = QColor("#170924"); t.hudBorder = QColor("#31134d");
      t.inputBg = QColor("#200c33"); t.inputBorder = QColor("#441a6b"); t.inputFocusBorder = QColor("#00ff66");
      t.buttonBg = QColor("#2a1045"); t.buttonHoverBg = QColor("#371559"); t.buttonPressedBg = QColor("#00cc52");
      t.buttonBorder = QColor("#521e85"); t.buttonText = QColor("#e6ffe6");
      t.textPrimary = QColor("#f0e6ff"); t.textSecondary = QColor("#a670e8");
      t.treeBg = QColor("#0d0514"); t.treeAltBg = QColor("#12071c"); t.treeItemHover = QColor("#240e38");
      t.treeItemSelected = QColor("#00ff66"); t.treeItemSelectedText = QColor("#0d0514");
      t.percentBarColor = QColor(0, 255, 102, 100);
      t.headerBg = QColor("#1f0c30"); t.headerText = QColor("#70ffab"); t.headerBorder = QColor("#31134d");
      t.progressBg = QColor("#170924"); t.progressChunk = QColor("#00ff66"); t.progressText = QColor("#0d0514"); t.progressBorder = QColor("#441a6b");
      t.menuBg = QColor("#170924"); t.menuText = QColor("#f0e6ff"); t.menuSelectedBg = QColor("#00ff66"); t.menuBorder = QColor("#441a6b");
      t.deleteEnabledBg = QColor("#ff0044"); t.deleteEnabledBorder = QColor("#ff4477");
      m_builtinPresets.append(t); }

    // 39. Vaporwave Pink (Retro 80s pink & teal pastel)
    { Theme t; t.name = "Vaporwave Pink";
      t.windowBg = QColor("#1a0c16"); t.panelBg = QColor("#261221");
      t.hudBg = QColor("#261221"); t.hudBorder = QColor("#4d2443");
      t.inputBg = QColor("#33182c"); t.inputBorder = QColor("#5e2d52"); t.inputFocusBorder = QColor("#ff71ce");
      t.buttonBg = QColor("#401f37"); t.buttonHoverBg = QColor("#522847"); t.buttonPressedBg = QColor("#cc4ea0");
      t.buttonBorder = QColor("#783a69"); t.buttonText = QColor("#ffe6f5");
      t.textPrimary = QColor("#fff0fa"); t.textSecondary = QColor("#cc7aa3");
      t.treeBg = QColor("#1a0c16"); t.treeAltBg = QColor("#210f1c"); t.treeItemHover = QColor("#381a30");
      t.treeItemSelected = QColor("#ff71ce"); t.treeItemSelectedText = QColor("#1a0c16");
      t.percentBarColor = QColor(1, 205, 254, 100);
      t.headerBg = QColor("#2e1628"); t.headerText = QColor("#01cdfe"); t.headerBorder = QColor("#4d2443");
      t.progressBg = QColor("#261221"); t.progressChunk = QColor("#ff71ce"); t.progressText = QColor("#1a0c16"); t.progressBorder = QColor("#5e2d52");
      t.menuBg = QColor("#261221"); t.menuText = QColor("#fff0fa"); t.menuSelectedBg = QColor("#ff71ce"); t.menuBorder = QColor("#5e2d52");
      t.deleteEnabledBg = QColor("#ff0055"); t.deleteEnabledBorder = QColor("#ff5588");
      m_builtinPresets.append(t); }

    // 40. Honeycomb (Sweet honey yellow & warm brown)
    { Theme t; t.name = "Honeycomb";
      t.windowBg = QColor("#1f190e"); t.panelBg = QColor("#2d2414");
      t.hudBg = QColor("#2d2414"); t.hudBorder = QColor("#544325");
      t.inputBg = QColor("#3b2f1a"); t.inputBorder = QColor("#69542e"); t.inputFocusBorder = QColor("#ffc107");
      t.buttonBg = QColor("#473920"); t.buttonHoverBg = QColor("#5a4829"); t.buttonPressedBg = QColor("#cc9a05");
      t.buttonBorder = QColor("#806738"); t.buttonText = QColor("#fff4d6");
      t.textPrimary = QColor("#fff9eb"); t.textSecondary = QColor("#cca86b");
      t.treeBg = QColor("#1f190e"); t.treeAltBg = QColor("#261f12"); t.treeItemHover = QColor("#40331c");
      t.treeItemSelected = QColor("#ffc107"); t.treeItemSelectedText = QColor("#1f190e");
      t.percentBarColor = QColor(255, 193, 7, 100);
      t.headerBg = QColor("#332917"); t.headerText = QColor("#ffe082"); t.headerBorder = QColor("#544325");
      t.progressBg = QColor("#2d2414"); t.progressChunk = QColor("#ffc107"); t.progressText = QColor("#1f190e"); t.progressBorder = QColor("#69542e");
      t.menuBg = QColor("#2d2414"); t.menuText = QColor("#fff9eb"); t.menuSelectedBg = QColor("#ffc107"); t.menuBorder = QColor("#69542e");
      t.deleteEnabledBg = QColor("#d32f2f"); t.deleteEnabledBorder = QColor("#ef5350");
      m_builtinPresets.append(t); }

    // 41. Inferno Red (Scorching lava red & orange)
    { Theme t; t.name = "Inferno Red";
      t.windowBg = QColor("#140000"); t.panelBg = QColor("#210000");
      t.hudBg = QColor("#210000"); t.hudBorder = QColor("#470000");
      t.inputBg = QColor("#2e0000"); t.inputBorder = QColor("#5c0000"); t.inputFocusBorder = QColor("#ff1100");
      t.buttonBg = QColor("#3b0000"); t.buttonHoverBg = QColor("#4d0000"); t.buttonPressedBg = QColor("#990a00");
      t.buttonBorder = QColor("#750000"); t.buttonText = QColor("#ffe0e0");
      t.textPrimary = QColor("#ffeded"); t.textSecondary = QColor("#cc6666");
      t.treeBg = QColor("#140000"); t.treeAltBg = QColor("#1c0000"); t.treeItemHover = QColor("#330000");
      t.treeItemSelected = QColor("#ff1100"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(255, 68, 0, 100);
      t.headerBg = QColor("#290000"); t.headerText = QColor("#ff8080"); t.headerBorder = QColor("#470000");
      t.progressBg = QColor("#210000"); t.progressChunk = QColor("#ff1100"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#5c0000");
      t.menuBg = QColor("#210000"); t.menuText = QColor("#ffeded"); t.menuSelectedBg = QColor("#ff1100"); t.menuBorder = QColor("#5c0000");
      t.deleteEnabledBg = QColor("#cc0000"); t.deleteEnabledBorder = QColor("#ff3333");
      m_builtinPresets.append(t); }

    // 42. Jade Forest (Deep jade stone green)
    { Theme t; t.name = "Jade Forest";
      t.windowBg = QColor("#081410"); t.panelBg = QColor("#0e211b");
      t.hudBg = QColor("#0e211b"); t.hudBorder = QColor("#1c4236");
      t.inputBg = QColor("#132e26"); t.inputBorder = QColor("#235445"); t.inputFocusBorder = QColor("#00cc88");
      t.buttonBg = QColor("#183b30"); t.buttonHoverBg = QColor("#204c3e"); t.buttonPressedBg = QColor("#008a5c");
      t.buttonBorder = QColor("#2d6b57"); t.buttonText = QColor("#d6fff2");
      t.textPrimary = QColor("#e6fff7"); t.textSecondary = QColor("#66cca8");
      t.treeBg = QColor("#081410"); t.treeAltBg = QColor("#0b1c17"); t.treeItemHover = QColor("#15382e");
      t.treeItemSelected = QColor("#00cc88"); t.treeItemSelectedText = QColor("#081410");
      t.percentBarColor = QColor(0, 204, 136, 100);
      t.headerBg = QColor("#112921"); t.headerText = QColor("#80ffcf"); t.headerBorder = QColor("#1c4236");
      t.progressBg = QColor("#0e211b"); t.progressChunk = QColor("#00cc88"); t.progressText = QColor("#081410"); t.progressBorder = QColor("#235445");
      t.menuBg = QColor("#0e211b"); t.menuText = QColor("#e6fff7"); t.menuSelectedBg = QColor("#00cc88"); t.menuBorder = QColor("#235445");
      t.deleteEnabledBg = QColor("#e53935"); t.deleteEnabledBorder = QColor("#ef5350");
      m_builtinPresets.append(t); }

    // 43. Electric Canary (Bright electric yellow & dark slate)
    { Theme t; t.name = "Electric Canary";
      t.windowBg = QColor("#141410"); t.panelBg = QColor("#1f1f18");
      t.hudBg = QColor("#1f1f18"); t.hudBorder = QColor("#3b3b2e");
      t.inputBg = QColor("#292920"); t.inputBorder = QColor("#4d4d3b"); t.inputFocusBorder = QColor("#facc15");
      t.buttonBg = QColor("#333328"); t.buttonHoverBg = QColor("#424234"); t.buttonPressedBg = QColor("#cc9f00");
      t.buttonBorder = QColor("#61614d"); t.buttonText = QColor("#fef08a");
      t.textPrimary = QColor("#fefce8"); t.textSecondary = QColor("#ca8a04");
      t.treeBg = QColor("#141410"); t.treeAltBg = QColor("#1a1a15"); t.treeItemHover = QColor("#2e2e24");
      t.treeItemSelected = QColor("#facc15"); t.treeItemSelectedText = QColor("#141410");
      t.percentBarColor = QColor(250, 204, 21, 100);
      t.headerBg = QColor("#24241c"); t.headerText = QColor("#fde047"); t.headerBorder = QColor("#3b3b2e");
      t.progressBg = QColor("#1f1f18"); t.progressChunk = QColor("#facc15"); t.progressText = QColor("#141410"); t.progressBorder = QColor("#4d4d3b");
      t.menuBg = QColor("#1f1f18"); t.menuText = QColor("#fefce8"); t.menuSelectedBg = QColor("#facc15"); t.menuBorder = QColor("#4d4d3b");
      t.deleteEnabledBg = QColor("#dc2626"); t.deleteEnabledBorder = QColor("#f87171");
      m_builtinPresets.append(t); }

    // 44. Magma Chamber (Fiery red, orange & volcanic dark gray)
    { Theme t; t.name = "Magma Chamber";
      t.windowBg = QColor("#140806"); t.panelBg = QColor("#210f0a");
      t.hudBg = QColor("#210f0a"); t.hudBorder = QColor("#421d14");
      t.inputBg = QColor("#2e150e"); t.inputBorder = QColor("#54271a"); t.inputFocusBorder = QColor("#ff3300");
      t.buttonBg = QColor("#3b1b12"); t.buttonHoverBg = QColor("#4c2317"); t.buttonPressedBg = QColor("#cc2900");
      t.buttonBorder = QColor("#6e3120"); t.buttonText = QColor("#ffe3db");
      t.textPrimary = QColor("#fff0eb"); t.textSecondary = QColor("#cc7a66");
      t.treeBg = QColor("#140806"); t.treeAltBg = QColor("#1a0b08"); t.treeItemHover = QColor("#331710");
      t.treeItemSelected = QColor("#ff3300"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(255, 102, 0, 100);
      t.headerBg = QColor("#29120c"); t.headerText = QColor("#ff8c66"); t.headerBorder = QColor("#421d14");
      t.progressBg = QColor("#210f0a"); t.progressChunk = QColor("#ff3300"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#54271a");
      t.menuBg = QColor("#210f0a"); t.menuText = QColor("#fff0eb"); t.menuSelectedBg = QColor("#ff3300"); t.menuBorder = QColor("#54271a");
      t.deleteEnabledBg = QColor("#b30000"); t.deleteEnabledBorder = QColor("#ff3333");
      m_builtinPresets.append(t); }

    // 45. Alien Matrix (Neon green & dark cyan extraterrestrial)
    { Theme t; t.name = "Alien Matrix";
      t.windowBg = QColor("#02120d"); t.panelBg = QColor("#041f17");
      t.hudBg = QColor("#041f17"); t.hudBorder = QColor("#093d2e");
      t.inputBg = QColor("#062b20"); t.inputBorder = QColor("#0e4d3b"); t.inputFocusBorder = QColor("#00ff88");
      t.buttonBg = QColor("#08382b"); t.buttonHoverBg = QColor("#0b4737"); t.buttonPressedBg = QColor("#00b35f");
      t.buttonBorder = QColor("#11664f"); t.buttonText = QColor("#d6ffec");
      t.textPrimary = QColor("#e6fff3"); t.textSecondary = QColor("#4db885");
      t.treeBg = QColor("#02120d"); t.treeAltBg = QColor("#031711"); t.treeItemHover = QColor("#073024");
      t.treeItemSelected = QColor("#00ff88"); t.treeItemSelectedText = QColor("#02120d");
      t.percentBarColor = QColor(0, 255, 136, 100);
      t.headerBg = QColor("#05241b"); t.headerText = QColor("#66ffb3"); t.headerBorder = QColor("#093d2e");
      t.progressBg = QColor("#041f17"); t.progressChunk = QColor("#00ff88"); t.progressText = QColor("#02120d"); t.progressBorder = QColor("#0e4d3b");
      t.menuBg = QColor("#041f17"); t.menuText = QColor("#e6fff3"); t.menuSelectedBg = QColor("#00ff88"); t.menuBorder = QColor("#0e4d3b");
      t.deleteEnabledBg = QColor("#ff0044"); t.deleteEnabledBorder = QColor("#ff4477");
      m_builtinPresets.append(t); }

    // 46. Sunshine Glow (Warm yellow & golden cream)
    { Theme t; t.name = "Sunshine Glow";
      t.windowBg = QColor("#1c1a12"); t.panelBg = QColor("#29261a");
      t.hudBg = QColor("#29261a"); t.hudBorder = QColor("#4a452e");
      t.inputBg = QColor("#363222"); t.inputBorder = QColor("#5e5838"); t.inputFocusBorder = QColor("#ffea00");
      t.buttonBg = QColor("#423e29"); t.buttonHoverBg = QColor("#544f34"); t.buttonPressedBg = QColor("#cca300");
      t.buttonBorder = QColor("#756e49"); t.buttonText = QColor("#fffbcf");
      t.textPrimary = QColor("#fffde6"); t.textSecondary = QColor("#c2b870");
      t.treeBg = QColor("#1c1a12"); t.treeAltBg = QColor("#242117"); t.treeItemHover = QColor("#3b3724");
      t.treeItemSelected = QColor("#ffea00"); t.treeItemSelectedText = QColor("#1c1a12");
      t.percentBarColor = QColor(255, 234, 0, 100);
      t.headerBg = QColor("#302c1e"); t.headerText = QColor("#fff176"); t.headerBorder = QColor("#4a452e");
      t.progressBg = QColor("#29261a"); t.progressChunk = QColor("#ffea00"); t.progressText = QColor("#1c1a12"); t.progressBorder = QColor("#5e5838");
      t.menuBg = QColor("#29261a"); t.menuText = QColor("#fffde6"); t.menuSelectedBg = QColor("#ffea00"); t.menuBorder = QColor("#5e5838");
      t.deleteEnabledBg = QColor("#ff3d00"); t.deleteEnabledBorder = QColor("#ff6e40");
      m_builtinPresets.append(t); }

    // 47. Scarlet Crimson (Lustrous deep red & ruby)
    { Theme t; t.name = "Scarlet Crimson";
      t.windowBg = QColor("#170205"); t.panelBg = QColor("#240409");
      t.hudBg = QColor("#240409"); t.hudBorder = QColor("#4a0812");
      t.inputBg = QColor("#30050c"); t.inputBorder = QColor("#5c0b17"); t.inputFocusBorder = QColor("#d50000");
      t.buttonBg = QColor("#3d070f"); t.buttonHoverBg = QColor("#4f0913"); t.buttonPressedBg = QColor("#990000");
      t.buttonBorder = QColor("#780e1d"); t.buttonText = QColor("#ffccd3");
      t.textPrimary = QColor("#ffe6ea"); t.textSecondary = QColor("#cc5c6e");
      t.treeBg = QColor("#170205"); t.treeAltBg = QColor("#1f0307"); t.treeItemHover = QColor("#36060e");
      t.treeItemSelected = QColor("#d50000"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(213, 0, 0, 100);
      t.headerBg = QColor("#2c050b"); t.headerText = QColor("#ff7388"); t.headerBorder = QColor("#4a0812");
      t.progressBg = QColor("#240409"); t.progressChunk = QColor("#d50000"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#5c0b17");
      t.menuBg = QColor("#240409"); t.menuText = QColor("#ffe6ea"); t.menuSelectedBg = QColor("#d50000"); t.menuBorder = QColor("#5c0b17");
      t.deleteEnabledBg = QColor("#990000"); t.deleteEnabledBorder = QColor("#e60000");
      m_builtinPresets.append(t); }

    // 48. Biohazard Green (Vivid yellow-green warning sign)
    { Theme t; t.name = "Biohazard Green";
      t.windowBg = QColor("#0d1405"); t.panelBg = QColor("#162109");
      t.hudBg = QColor("#162109"); t.hudBorder = QColor("#2d4212");
      t.inputBg = QColor("#1f2e0c"); t.inputBorder = QColor("#3a5417"); t.inputFocusBorder = QColor("#a6ff00");
      t.buttonBg = QColor("#273b0f"); t.buttonHoverBg = QColor("#334d14"); t.buttonPressedBg = QColor("#75b300");
      t.buttonBorder = QColor("#486e1c"); t.buttonText = QColor("#eeffcc");
      t.textPrimary = QColor("#f4ffe6"); t.textSecondary = QColor("#99cc55");
      t.treeBg = QColor("#0d1405"); t.treeAltBg = QColor("#121c07"); t.treeItemHover = QColor("#22330d");
      t.treeItemSelected = QColor("#a6ff00"); t.treeItemSelectedText = QColor("#0d1405");
      t.percentBarColor = QColor(166, 255, 0, 100);
      t.headerBg = QColor("#1b290b"); t.headerText = QColor("#c4ff4d"); t.headerBorder = QColor("#2d4212");
      t.progressBg = QColor("#162109"); t.progressChunk = QColor("#a6ff00"); t.progressText = QColor("#0d1405"); t.progressBorder = QColor("#3a5417");
      t.menuBg = QColor("#162109"); t.menuText = QColor("#f4ffe6"); t.menuSelectedBg = QColor("#a6ff00"); t.menuBorder = QColor("#3a5417");
      t.deleteEnabledBg = QColor("#ff002a"); t.deleteEnabledBorder = QColor("#ff4466");
      m_builtinPresets.append(t); }

    // 49. Amber Alert (High contrast amber yellow & black)
    { Theme t; t.name = "Amber Alert";
      t.windowBg = QColor("#141005"); t.panelBg = QColor("#211a08");
      t.hudBg = QColor("#211a08"); t.hudBorder = QColor("#423410");
      t.inputBg = QColor("#2e240b"); t.inputBorder = QColor("#544215"); t.inputFocusBorder = QColor("#ffaa00");
      t.buttonBg = QColor("#3b2e0e"); t.buttonHoverBg = QColor("#4c3b12"); t.buttonPressedBg = QColor("#cc8800");
      t.buttonBorder = QColor("#6e551a"); t.buttonText = QColor("#ffeed4");
      t.textPrimary = QColor("#fff4e6"); t.textSecondary = QColor("#cca859");
      t.treeBg = QColor("#141005"); t.treeAltBg = QColor("#1a1607"); t.treeItemHover = QColor("#33280c");
      t.treeItemSelected = QColor("#ffaa00"); t.treeItemSelectedText = QColor("#141005");
      t.percentBarColor = QColor(255, 170, 0, 100);
      t.headerBg = QColor("#29200a"); t.headerText = QColor("#ffcc66"); t.headerBorder = QColor("#423410");
      t.progressBg = QColor("#211a08"); t.progressChunk = QColor("#ffaa00"); t.progressText = QColor("#141005"); t.progressBorder = QColor("#544215");
      t.menuBg = QColor("#211a08"); t.menuText = QColor("#fff4e6"); t.menuSelectedBg = QColor("#ffaa00"); t.menuBorder = QColor("#544215");
      t.deleteEnabledBg = QColor("#d60000"); t.deleteEnabledBorder = QColor("#ff3333");
      m_builtinPresets.append(t); }

    // 50. Chili Pepper (Spicy bright red & dark maroon)
    { Theme t; t.name = "Chili Pepper";
      t.windowBg = QColor("#190808"); t.panelBg = QColor("#260d0d");
      t.hudBg = QColor("#260d0d"); t.hudBorder = QColor("#4d1a1a");
      t.inputBg = QColor("#331212"); t.inputBorder = QColor("#5e2121"); t.inputFocusBorder = QColor("#ff1a1a");
      t.buttonBg = QColor("#401717"); t.buttonHoverBg = QColor("#521d1d"); t.buttonPressedBg = QColor("#cc0000");
      t.buttonBorder = QColor("#782b2b"); t.buttonText = QColor("#ffe6e6");
      t.textPrimary = QColor("#fff0f0"); t.textSecondary = QColor("#cc8080");
      t.treeBg = QColor("#190808"); t.treeAltBg = QColor("#210b0b"); t.treeItemHover = QColor("#381414");
      t.treeItemSelected = QColor("#ff1a1a"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(255, 26, 26, 100);
      t.headerBg = QColor("#2e0f0f"); t.headerText = QColor("#ff8080"); t.headerBorder = QColor("#4d1a1a");
      t.progressBg = QColor("#260d0d"); t.progressChunk = QColor("#ff1a1a"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#5e2121");
      t.menuBg = QColor("#260d0d"); t.menuText = QColor("#fff0f0"); t.menuSelectedBg = QColor("#ff1a1a"); t.menuBorder = QColor("#5e2121");
      t.deleteEnabledBg = QColor("#cc0000"); t.deleteEnabledBorder = QColor("#ff3333");
      m_builtinPresets.append(t); }

    // 51. Spring Meadow (Lush bright spring green & white)
    { Theme t; t.name = "Spring Meadow";
      t.windowBg = QColor("#08170c"); t.panelBg = QColor("#0d2413");
      t.hudBg = QColor("#0d2413"); t.hudBorder = QColor("#194726");
      t.inputBg = QColor("#12301a"); t.inputBorder = QColor("#21572e"); t.inputFocusBorder = QColor("#22c55e");
      t.buttonBg = QColor("#163d20"); t.buttonHoverBg = QColor("#1e4f2a"); t.buttonPressedBg = QColor("#15803d");
      t.buttonBorder = QColor("#2a6e3b"); t.buttonText = QColor("#dcffe4");
      t.textPrimary = QColor("#eafef0"); t.textSecondary = QColor("#6ee7b7");
      t.treeBg = QColor("#08170c"); t.treeAltBg = QColor("#0b1f10"); t.treeItemHover = QColor("#143a1f");
      t.treeItemSelected = QColor("#22c55e"); t.treeItemSelectedText = QColor("#08170c");
      t.percentBarColor = QColor(34, 197, 94, 100);
      t.headerBg = QColor("#102b17"); t.headerText = QColor("#86efac"); t.headerBorder = QColor("#194726");
      t.progressBg = QColor("#0d2413"); t.progressChunk = QColor("#22c55e"); t.progressText = QColor("#08170c"); t.progressBorder = QColor("#21572e");
      t.menuBg = QColor("#0d2413"); t.menuText = QColor("#eafef0"); t.menuSelectedBg = QColor("#22c55e"); t.menuBorder = QColor("#21572e");
      t.deleteEnabledBg = QColor("#ef4444"); t.deleteEnabledBorder = QColor("#f87171");
      m_builtinPresets.append(t); }

    // 52. Gold Dust (Sparkling champagne gold & onyx)
    { Theme t; t.name = "Gold Dust";
      t.windowBg = QColor("#17140c"); t.panelBg = QColor("#241f13");
      t.hudBg = QColor("#241f13"); t.hudBorder = QColor("#473d26");
      t.inputBg = QColor("#30291a"); t.inputBorder = QColor("#594d30"); t.inputFocusBorder = QColor("#d4af37");
      t.buttonBg = QColor("#3d3421"); t.buttonHoverBg = QColor("#4f432b"); t.buttonPressedBg = QColor("#a68824");
      t.buttonBorder = QColor("#70603d"); t.buttonText = QColor("#fceec7");
      t.textPrimary = QColor("#fdf8e7"); t.textSecondary = QColor("#c7b27c");
      t.treeBg = QColor("#17140c"); t.treeAltBg = QColor("#1f1a10"); t.treeItemHover = QColor("#362e1d");
      t.treeItemSelected = QColor("#d4af37"); t.treeItemSelectedText = QColor("#17140c");
      t.percentBarColor = QColor(212, 175, 55, 100);
      t.headerBg = QColor("#2b2517"); t.headerText = QColor("#f5d77f"); t.headerBorder = QColor("#473d26");
      t.progressBg = QColor("#241f13"); t.progressChunk = QColor("#d4af37"); t.progressText = QColor("#17140c"); t.progressBorder = QColor("#594d30");
      t.menuBg = QColor("#241f13"); t.menuText = QColor("#fdf8e7"); t.menuSelectedBg = QColor("#d4af37"); t.menuBorder = QColor("#594d30");
      t.deleteEnabledBg = QColor("#c62828"); t.deleteEnabledBorder = QColor("#ef5350");
      m_builtinPresets.append(t); }

    // 53. Lava Flow (Molten orange-red & ash gray)
    { Theme t; t.name = "Lava Flow";
      t.windowBg = QColor("#140a04"); t.panelBg = QColor("#211107");
      t.hudBg = QColor("#211107"); t.hudBorder = QColor("#42220e");
      t.inputBg = QColor("#2e180a"); t.inputBorder = QColor("#542c12"); t.inputFocusBorder = QColor("#ff5500");
      t.buttonBg = QColor("#3b1f0d"); t.buttonHoverBg = QColor("#4c2811"); t.buttonPressedBg = QColor("#cc4400");
      t.buttonBorder = QColor("#6e3a19"); t.buttonText = QColor("#ffe5d6");
      t.textPrimary = QColor("#fff1e6"); t.textSecondary = QColor("#cc8866");
      t.treeBg = QColor("#140a04"); t.treeAltBg = QColor("#1a0d05"); t.treeItemHover = QColor("#331a0b");
      t.treeItemSelected = QColor("#ff5500"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(255, 85, 0, 100);
      t.headerBg = QColor("#291509"); t.headerText = QColor("#ff9966"); t.headerBorder = QColor("#42220e");
      t.progressBg = QColor("#211107"); t.progressChunk = QColor("#ff5500"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#542c12");
      t.menuBg = QColor("#211107"); t.menuText = QColor("#fff1e6"); t.menuSelectedBg = QColor("#ff5500"); t.menuBorder = QColor("#542c12");
      t.deleteEnabledBg = QColor("#b30000"); t.deleteEnabledBorder = QColor("#ff3333");
      m_builtinPresets.append(t); }

    // 54. Clover Field (Bright Irish clover green)
    { Theme t; t.name = "Clover Field";
      t.windowBg = QColor("#061408"); t.panelBg = QColor("#0a210e");
      t.hudBg = QColor("#0a210e"); t.hudBorder = QColor("#14421b");
      t.inputBg = QColor("#0e2e13"); t.inputBorder = QColor("#1a5422"); t.inputFocusBorder = QColor("#00e640");
      t.buttonBg = QColor("#123b18"); t.buttonHoverBg = QColor("#184c1f"); t.buttonPressedBg = QColor("#00b332");
      t.buttonBorder = QColor("#226e2d"); t.buttonText = QColor("#d6ffd9");
      t.textPrimary = QColor("#e6ffe8"); t.textSecondary = QColor("#66cc73");
      t.treeBg = QColor("#061408"); t.treeAltBg = QColor("#081c0b"); t.treeItemHover = QColor("#0f3616");
      t.treeItemSelected = QColor("#00e640"); t.treeItemSelectedText = QColor("#061408");
      t.percentBarColor = QColor(0, 230, 64, 100);
      t.headerBg = QColor("#0c2610"); t.headerText = QColor("#80ff95"); t.headerBorder = QColor("#14421b");
      t.progressBg = QColor("#0a210e"); t.progressChunk = QColor("#00e640"); t.progressText = QColor("#061408"); t.progressBorder = QColor("#1a5422");
      t.menuBg = QColor("#0a210e"); t.menuText = QColor("#e6ffe8"); t.menuSelectedBg = QColor("#00e640"); t.menuBorder = QColor("#1a5422");
      t.deleteEnabledBg = QColor("#d50000"); t.deleteEnabledBorder = QColor("#ff3333");
      m_builtinPresets.append(t); }

    // 55. Saffron Glow (Exotic saffron yellow & deep brown)
    { Theme t; t.name = "Saffron Glow";
      t.windowBg = QColor("#191409"); t.panelBg = QColor("#261e0e");
      t.hudBg = QColor("#261e0e"); t.hudBorder = QColor("#4d3c1c");
      t.inputBg = QColor("#332813"); t.inputBorder = QColor("#5e4a23"); t.inputFocusBorder = QColor("#ffc400");
      t.buttonBg = QColor("#403218"); t.buttonHoverBg = QColor("#52401f"); t.buttonPressedBg = QColor("#cc9d00");
      t.buttonBorder = QColor("#785d2d"); t.buttonText = QColor("#fff3d6");
      t.textPrimary = QColor("#fff8e6"); t.textSecondary = QColor("#cca859");
      t.treeBg = QColor("#191409"); t.treeAltBg = QColor("#211a0c"); t.treeItemHover = QColor("#382b15");
      t.treeItemSelected = QColor("#ffc400"); t.treeItemSelectedText = QColor("#191409");
      t.percentBarColor = QColor(255, 196, 0, 100);
      t.headerBg = QColor("#2e2411"); t.headerText = QColor("#ffe066"); t.headerBorder = QColor("#4d3c1c");
      t.progressBg = QColor("#261e0e"); t.progressChunk = QColor("#ffc400"); t.progressText = QColor("#191409"); t.progressBorder = QColor("#5e4a23");
      t.menuBg = QColor("#261e0e"); t.menuText = QColor("#fff8e6"); t.menuSelectedBg = QColor("#ffc400"); t.menuBorder = QColor("#5e4a23");
      t.deleteEnabledBg = QColor("#dd2c00"); t.deleteEnabledBorder = QColor("#ff5722");
      m_builtinPresets.append(t); }

    // 56. Cherry Blossom (Hot pink & soft rose dark)
    { Theme t; t.name = "Cherry Blossom";
      t.windowBg = QColor("#1a0810"); t.panelBg = QColor("#260c18");
      t.hudBg = QColor("#260c18"); t.hudBorder = QColor("#4d1930");
      t.inputBg = QColor("#331020"); t.inputBorder = QColor("#5e1e3b"); t.inputFocusBorder = QColor("#ff2a75");
      t.buttonBg = QColor("#401428"); t.buttonHoverBg = QColor("#521a33"); t.buttonPressedBg = QColor("#cc1154");
      t.buttonBorder = QColor("#78264b"); t.buttonText = QColor("#ffe6f0");
      t.textPrimary = QColor("#fff0f5"); t.textSecondary = QColor("#cc7799");
      t.treeBg = QColor("#1a0810"); t.treeAltBg = QColor("#210a14"); t.treeItemHover = QColor("#381223");
      t.treeItemSelected = QColor("#ff2a75"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(255, 42, 117, 100);
      t.headerBg = QColor("#2e0e1d"); t.headerText = QColor("#ff80b0"); t.headerBorder = QColor("#4d1930");
      t.progressBg = QColor("#260c18"); t.progressChunk = QColor("#ff2a75"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#5e1e3b");
      t.menuBg = QColor("#260c18"); t.menuText = QColor("#fff0f5"); t.menuSelectedBg = QColor("#ff2a75"); t.menuBorder = QColor("#5e1e3b");
      t.deleteEnabledBg = QColor("#c51162"); t.deleteEnabledBorder = QColor("#f50057");
      m_builtinPresets.append(t); }

    // 57. Neon Citrus (Bright lime & lemon yellow hybrid)
    { Theme t; t.name = "Neon Citrus";
      t.windowBg = QColor("#121405"); t.panelBg = QColor("#1d2108");
      t.hudBg = QColor("#1d2108"); t.hudBorder = QColor("#3a4210");
      t.inputBg = QColor("#272c0b"); t.inputBorder = QColor("#495415"); t.inputFocusBorder = QColor("#d4ff00");
      t.buttonBg = QColor("#31380e"); t.buttonHoverBg = QColor("#3f4712"); t.buttonPressedBg = QColor("#99b800");
      t.buttonBorder = QColor("#5b691a"); t.buttonText = QColor("#f7ffcc");
      t.textPrimary = QColor("#fbffe6"); t.textSecondary = QColor("#b8cc52");
      t.treeBg = QColor("#121405"); t.treeAltBg = QColor("#181c07"); t.treeItemHover = QColor("#2b300c");
      t.treeItemSelected = QColor("#d4ff00"); t.treeItemSelectedText = QColor("#121405");
      t.percentBarColor = QColor(212, 255, 0, 100);
      t.headerBg = QColor("#232909"); t.headerText = QColor("#e5ff66"); t.headerBorder = QColor("#3a4210");
      t.progressBg = QColor("#1d2108"); t.progressChunk = QColor("#d4ff00"); t.progressText = QColor("#121405"); t.progressBorder = QColor("#495415");
      t.menuBg = QColor("#1d2108"); t.menuText = QColor("#fbffe6"); t.menuSelectedBg = QColor("#d4ff00"); t.menuBorder = QColor("#495415");
      t.deleteEnabledBg = QColor("#ff1744"); t.deleteEnabledBorder = QColor("#ff616f");
      m_builtinPresets.append(t); }

    // 58. Velvet Purple (Deep regal violet & magenta)
    { Theme t; t.name = "Velvet Purple";
      t.windowBg = QColor("#120617"); t.panelBg = QColor("#1c0924");
      t.hudBg = QColor("#1c0924"); t.hudBorder = QColor("#39124a");
      t.inputBg = QColor("#250c30"); t.inputBorder = QColor("#46175c"); t.inputFocusBorder = QColor("#b100e6");
      t.buttonBg = QColor("#300f3d"); t.buttonHoverBg = QColor("#3e144f"); t.buttonPressedBg = QColor("#8500ad");
      t.buttonBorder = QColor("#5c1d78"); t.buttonText = QColor("#f8e0ff");
      t.textPrimary = QColor("#fce6ff"); t.textSecondary = QColor("#c470e8");
      t.treeBg = QColor("#120617"); t.treeAltBg = QColor("#17081e"); t.treeItemHover = QColor("#2b0d38");
      t.treeItemSelected = QColor("#b100e6"); t.treeItemSelectedText = QColor("#ffffff");
      t.percentBarColor = QColor(177, 0, 230, 100);
      t.headerBg = QColor("#220b2c"); t.headerText = QColor("#e880ff"); t.headerBorder = QColor("#39124a");
      t.progressBg = QColor("#1c0924"); t.progressChunk = QColor("#b100e6"); t.progressText = QColor("#ffffff"); t.progressBorder = QColor("#46175c");
      t.menuBg = QColor("#1c0924"); t.menuText = QColor("#fce6ff"); t.menuSelectedBg = QColor("#b100e6"); t.menuBorder = QColor("#46175c");
      t.deleteEnabledBg = QColor("#d50000"); t.deleteEnabledBorder = QColor("#ff3333");
      m_builtinPresets.append(t); }

    // 59. Electric Sunburst (High contrast orange-yellow & dark slate)
    { Theme t; t.name = "Electric Sunburst";
      t.windowBg = QColor("#171206"); t.panelBg = QColor("#241c09");
      t.hudBg = QColor("#241c09"); t.hudBorder = QColor("#473712");
      t.inputBg = QColor("#30250c"); t.inputBorder = QColor("#594517"); t.inputFocusBorder = QColor("#ff8c00");
      t.buttonBg = QColor("#3d2f0f"); t.buttonHoverBg = QColor("#4f3d13"); t.buttonPressedBg = QColor("#cc7000");
      t.buttonBorder = QColor("#70571a"); t.buttonText = QColor("#ffe8c7");
      t.textPrimary = QColor("#fff3e0"); t.textSecondary = QColor("#cca059");
      t.treeBg = QColor("#171206"); t.treeAltBg = QColor("#1f1808"); t.treeItemHover = QColor("#362a0d");
      t.treeItemSelected = QColor("#ff8c00"); t.treeItemSelectedText = QColor("#171206");
      t.percentBarColor = QColor(255, 140, 0, 100);
      t.headerBg = QColor("#2b210b"); t.headerText = QColor("#ffc066"); t.headerBorder = QColor("#473712");
      t.progressBg = QColor("#241c09"); t.progressChunk = QColor("#ff8c00"); t.progressText = QColor("#171206"); t.progressBorder = QColor("#594517");
      t.menuBg = QColor("#241c09"); t.menuText = QColor("#fff3e0"); t.menuSelectedBg = QColor("#ff8c00"); t.menuBorder = QColor("#594517");
      t.deleteEnabledBg = QColor("#dd2c00"); t.deleteEnabledBorder = QColor("#ff5722");
      m_builtinPresets.append(t); }

    // 60. Cyber Emerald (Neon green matrix glow on obsidian)
    { Theme t; t.name = "Cyber Emerald";
      t.windowBg = QColor("#02140a"); t.panelBg = QColor("#052112");
      t.hudBg = QColor("#052112"); t.hudBorder = QColor("#0a4224");
      t.inputBg = QColor("#072e19"); t.inputBorder = QColor("#0e522d"); t.inputFocusBorder = QColor("#00ff66");
      t.buttonBg = QColor("#0a3d21"); t.buttonHoverBg = QColor("#0d4d2a"); t.buttonPressedBg = QColor("#00cc52");
      t.buttonBorder = QColor("#136e3c"); t.buttonText = QColor("#d6ffea");
      t.textPrimary = QColor("#e6fff2"); t.textSecondary = QColor("#52cc8a");
      t.treeBg = QColor("#02140a"); t.treeAltBg = QColor("#031b0e"); t.treeItemHover = QColor("#07361c");
      t.treeItemSelected = QColor("#00ff66"); t.treeItemSelectedText = QColor("#02140a");
      t.percentBarColor = QColor(0, 255, 102, 100);
      t.headerBg = QColor("#062916"); t.headerText = QColor("#66ffa8"); t.headerBorder = QColor("#0a4224");
      t.progressBg = QColor("#052112"); t.progressChunk = QColor("#00ff66"); t.progressText = QColor("#02140a"); t.progressBorder = QColor("#0e522d");
      t.menuBg = QColor("#052112"); t.menuText = QColor("#e6fff2"); t.menuSelectedBg = QColor("#00ff66"); t.menuBorder = QColor("#0e522d");
      t.deleteEnabledBg = QColor("#ff0033"); t.deleteEnabledBorder = QColor("#ff4466");
      m_builtinPresets.append(t); }
}
