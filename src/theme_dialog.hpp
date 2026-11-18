// ==============================================================================
// Program: space_map - theme_dialog.hpp
// Programmer: Ethan-da-Tech-Wizard (written with Claude, an AI assistant)
// Last Modified: 2026-11-18
// Purpose: Declares ThemeDialog, the window for choosing theme presets and editing individual theme colors.
// ==============================================================================
#pragma once

#include <QDialog>
#include <QScrollArea>
#include <QMap>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include "theme_manager.hpp"

/// Dialog with a preset grid on the left and a per-color editor on the right.
/// Changes are applied live to the application as they are made.
class ThemeDialog : public QDialog {
    Q_OBJECT
public:
    /// Builds the dialog starting from the currently active theme.
    explicit ThemeDialog(QWidget* parent = nullptr);

// Slots for user actions in the dialog
private slots:
    /// Selects and applies a built-in or user preset.
    void onPresetClicked(int index, bool isUserPreset);
    /// Opens a color picker for the named theme field.
    void onColorButtonClicked(const QString& fieldName);
    /// Applies a color typed into the hex box when it is valid.
    void onHexEditFinished(const QString& fieldName);
    /// Saves the current edited colors as a new user preset.
    void onSaveUserPreset();
    /// Restores the theme that was active when the dialog opened.
    void onReset();

private:
    /// Creates the dialog's widgets and layouts.
    void setupUi();
    /// Applies the dialog's own stylesheet.
    void applyDialogStyle();
    /// Recreates the preset cards, built-in first then user presets.
    void rebuildPresetGrid();
    /// Builds one preset card with color swatches and a name.
    QWidget* createPresetCard(const Theme& theme, int index, bool isUserPreset);
    /// Adds a bold section heading to the color editor.
    void addColorSection(QVBoxLayout* layout, const QString& title);
    /// Adds one editor row: label, color button, hex box, and help text for a theme field.
    void addColorRow(QVBoxLayout* layout, const QString& label, const QString& fieldName);
    /// Copies a theme into the editor and refreshes every color button and hex box.
    void loadThemeIntoEditor(const Theme& theme);
    /// Pushes the theme being edited to the ThemeManager and the parent window.
    void applyEditingTheme();
    /// Returns a reference to the theme color matching a field name.
    QColor& getColorRef(const QString& fieldName);
    /// Repaints a color button with the given color.
    void updateColorButton(const QString& fieldName, const QColor& color);

    // Theme being edited (changes live here until saved or reset)
    Theme m_editingTheme;
    // Theme that was active when the dialog opened, used by Reset
    Theme m_originalTheme;

    QScrollArea* m_presetScroll = nullptr;
    QWidget* m_presetContainer = nullptr;
    QGridLayout* m_presetGrid = nullptr;
    QScrollArea* m_editorScroll = nullptr;

    // Editor widgets keyed by theme field name
    QMap<QString, QPushButton*> m_colorButtons;
    QMap<QString, QLineEdit*> m_hexInputs;

    QLabel* m_helpBanner = nullptr;

    // Which preset card is highlighted, and whether it is a user preset
    int m_selectedPresetIndex = 0;
    bool m_selectedIsUser = false;
};
