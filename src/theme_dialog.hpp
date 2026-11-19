// ==============================================================================
// theme_dialog.hpp - dialog for choosing and editing colour themes.
//
// Left side: preset cards (built-in and user-saved). Right side: one colour
// button plus hex field per Theme colour. Changes are previewed live through
// ThemeManager and can be reverted with m_originalTheme.
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

class ThemeDialog : public QDialog {
    Q_OBJECT
public:
    explicit ThemeDialog(QWidget* parent = nullptr);

private slots:
    void onPresetClicked(int index, bool isUserPreset);
    void onColorButtonClicked(const QString& fieldName);
    void onHexEditFinished(const QString& fieldName);
    void onSaveUserPreset();
    void onReset();

private:
    void setupUi();
    void applyDialogStyle();
    void rebuildPresetGrid();
    QWidget* createPresetCard(const Theme& theme, int index, bool isUserPreset);
    void addColorSection(QVBoxLayout* layout, const QString& title);
    void addColorRow(QVBoxLayout* layout, const QString& label, const QString& fieldName);
    void loadThemeIntoEditor(const Theme& theme);
    void applyEditingTheme();
    // Maps a field name (e.g. "windowBg") to the matching member of m_editingTheme
    // so the generic colour widgets can edit any colour.
    QColor& getColorRef(const QString& fieldName);
    void updateColorButton(const QString& fieldName, const QColor& color);

    // Working copy being edited; m_originalTheme is what to restore on reset.
    Theme m_editingTheme;
    Theme m_originalTheme;

    QScrollArea* m_presetScroll = nullptr;
    QWidget* m_presetContainer = nullptr;
    QGridLayout* m_presetGrid = nullptr;
    QScrollArea* m_editorScroll = nullptr;

    // Widgets keyed by the Theme field name they edit.
    QMap<QString, QPushButton*> m_colorButtons;
    QMap<QString, QLineEdit*> m_hexInputs;

    QLabel* m_helpBanner = nullptr;

    int m_selectedPresetIndex = 0;
    bool m_selectedIsUser = false;
};
