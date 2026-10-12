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
    QColor& getColorRef(const QString& fieldName);
    void updateColorButton(const QString& fieldName, const QColor& color);

    Theme m_editingTheme;
    Theme m_originalTheme;

    QScrollArea* m_presetScroll = nullptr;
    QWidget* m_presetContainer = nullptr;
    QGridLayout* m_presetGrid = nullptr;
    QScrollArea* m_editorScroll = nullptr;

    QMap<QString, QPushButton*> m_colorButtons;
    QMap<QString, QLineEdit*> m_hexInputs;

    QLabel* m_helpBanner = nullptr;

    int m_selectedPresetIndex = 0;
    bool m_selectedIsUser = false;
};
