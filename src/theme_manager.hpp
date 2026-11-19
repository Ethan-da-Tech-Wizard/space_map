// ==============================================================================
// theme_manager.hpp - colour themes for the whole application.
//
// Theme is a plain bag of named colours (one per styled UI element).
// ThemeManager is a singleton that owns the built-in presets, the user's saved
// presets (persisted with QSettings as JSON), the active theme, and generates
// the Qt stylesheet from a Theme. It emits themeChanged() so windows restyle.
// ==============================================================================
#pragma once

#include <QObject>
#include <QColor>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QSettings>

struct Theme {
    QString name;

    // Colours are grouped by the widget family they style.
    // Window & Panels
    QColor windowBg;
    QColor panelBg;
    QColor hudBg;
    QColor hudBorder;

    // Inputs
    QColor inputBg;
    QColor inputBorder;
    QColor inputFocusBorder;

    // Buttons
    QColor buttonBg;
    QColor buttonHoverBg;
    QColor buttonPressedBg;
    QColor buttonBorder;
    QColor buttonText;

    // Text
    QColor textPrimary;
    QColor textSecondary;

    // Tree View
    QColor treeBg;
    QColor treeAltBg;
    QColor treeItemHover;
    QColor treeItemSelected;
    QColor treeItemSelectedText;
    QColor percentBarColor;

    // Headers
    QColor headerBg;
    QColor headerText;
    QColor headerBorder;

    // Progress Bar
    QColor progressBg;
    QColor progressChunk;
    QColor progressText;
    QColor progressBorder;

    // Menus
    QColor menuBg;
    QColor menuText;
    QColor menuSelectedBg;
    QColor menuBorder;

    // Delete Button
    QColor deleteEnabledBg;
    QColor deleteEnabledBorder;
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    // Singleton accessor (the constructor is private).
    static ThemeManager& instance();

    const QVector<Theme>& builtinPresets() const;
    const QVector<Theme>& userPresets() const;
    const Theme& activeTheme() const;
    int activePresetIndex() const;
    bool isActiveUserPreset() const;

    void applyBuiltinPreset(int index);
    void applyUserPreset(int index);
    // Make `theme` the active one and notify listeners via themeChanged().
    void applyTheme(const Theme& theme);

    int saveUserPreset(const Theme& theme);
    void deleteUserPreset(int index);
    void renameUserPreset(int index, const QString& name);

    // Builds the application-wide Qt stylesheet (QSS) string from a Theme.
    QString generateStylesheet(const Theme& t) const;

    void saveSettings() const;
    void loadSettings();

    // Serialisation used for persistence and import/export of presets.
    QJsonObject themeToJson(const Theme& t) const;
    Theme themeFromJson(const QJsonObject& obj) const;

signals:
    void themeChanged();

private:
    ThemeManager();
    void initBuiltinPresets();

    QVector<Theme> m_builtinPresets;
    QVector<Theme> m_userPresets;
    Theme m_activeTheme;
    int m_activePresetIndex = 0;
    bool m_isUserPreset = false;
};
