// ==============================================================================
// Program: space_map - theme_manager.hpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Declares Theme (the set of UI colors) and ThemeManager, the singleton that holds presets and saves them.
// ==============================================================================
#pragma once

#include <QObject>
#include <QColor>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QSettings>

/// A complete set of named UI colors, grouped by the part of the interface they style.
struct Theme {
    QString name;

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

/// Singleton that owns built-in and user theme presets, tracks the active theme,
/// generates the application stylesheet, and saves or loads settings.
class ThemeManager : public QObject {
    Q_OBJECT
public:
    /// Returns the single shared instance.
    static ThemeManager& instance();

    /// Presets that ship with the application.
    const QVector<Theme>& builtinPresets() const;
    /// Presets the user has saved.
    const QVector<Theme>& userPresets() const;
    /// The theme currently in effect.
    const Theme& activeTheme() const;
    /// Index of the active preset within its list.
    int activePresetIndex() const;
    /// True when the active preset comes from the user list.
    bool isActiveUserPreset() const;

    /// Makes a built-in preset active and emits themeChanged.
    void applyBuiltinPreset(int index);
    /// Makes a user preset active and emits themeChanged.
    void applyUserPreset(int index);
    /// Applies an arbitrary theme (for example one being edited) without changing the preset index.
    void applyTheme(const Theme& theme);

    /// Adds a user preset (naming it if blank) and returns its index.
    int saveUserPreset(const Theme& theme);
    /// Removes a user preset by index.
    void deleteUserPreset(int index);
    /// Renames a user preset by index.
    void renameUserPreset(int index, const QString& name);

    /// Builds the full application stylesheet text from a theme's colors.
    QString generateStylesheet(const Theme& t) const;

    /// Persists the active selection and user presets to application settings.
    void saveSettings() const;
    /// Restores user presets and the active selection from settings.
    void loadSettings();

    /// Converts a theme to JSON for storage.
    QJsonObject themeToJson(const Theme& t) const;
    /// Rebuilds a theme from stored JSON.
    Theme themeFromJson(const QJsonObject& obj) const;

signals:
    // Emitted whenever the active theme changes
    void themeChanged();

private:
    // Private constructor: use instance()
    ThemeManager();
    /// Fills the built-in preset list.
    void initBuiltinPresets();

    QVector<Theme> m_builtinPresets;
    QVector<Theme> m_userPresets;
    Theme m_activeTheme;
    int m_activePresetIndex = 0;
    bool m_isUserPreset = false;
};
