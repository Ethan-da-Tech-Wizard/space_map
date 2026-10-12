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
    static ThemeManager& instance();

    const QVector<Theme>& builtinPresets() const;
    const QVector<Theme>& userPresets() const;
    const Theme& activeTheme() const;
    int activePresetIndex() const;
    bool isActiveUserPreset() const;

    void applyBuiltinPreset(int index);
    void applyUserPreset(int index);
    void applyTheme(const Theme& theme);

    int saveUserPreset(const Theme& theme);
    void deleteUserPreset(int index);
    void renameUserPreset(int index, const QString& name);

    QString generateStylesheet(const Theme& t) const;

    void saveSettings() const;
    void loadSettings();

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
