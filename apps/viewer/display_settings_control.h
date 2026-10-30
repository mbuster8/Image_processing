#pragma once

#include "viewer_core/display_settings.h"

#include <QWidget>

#include <array>
#include <functional>

class FixedRangePopup;
class QAction;
class QActionGroup;
class QMenu;
class QToolButton;

// The info strip's "Range: Auto ▾" and "Colours: Grey ▾" buttons, for whatever the view shows. Choosing an
// entry emits the new settings; the window keeps them per name and redraws.
class DisplaySettingsControl : public QWidget {
    Q_OBJECT

public:
    explicit DisplaySettingsControl(QWidget* parent = nullptr);

    // Called when the shown image or channel changes, never per frame. Quiet: emits nothing. Colours are
    // offered for single-channel images only.
    void setState(const viewer_core::DisplaySettings& settings, bool singleChannel);
    // Asked for the current Auto range only when Hold or Fixed… is chosen, so new frames never disturb it.
    void setAutoRangeProvider(std::function<viewer_core::DisplayRange()> provider);
    QMenu* rangeMenu() const; // the Image menu shows the same actions
    QMenu* coloursMenu() const;

signals:
    void settingsChanged(viewer_core::DisplaySettings settings);

private:
    void chooseRange(viewer_core::RangeMode mode);
    void chooseColours(viewer_core::ColourMap colours);
    void openFixedPopup();
    void applyFixed(viewer_core::DisplayRange range);
    void showSettings(); // the buttons' texts and the menus' ticks, from m_settings
    viewer_core::DisplayRange currentRange() const;

    viewer_core::DisplaySettings m_settings;
    std::function<viewer_core::DisplayRange()> m_autoRange;
    QToolButton* m_rangeButton;
    QToolButton* m_coloursButton;
    QMenu* m_rangeMenu;
    QMenu* m_coloursMenu;
    std::array<QAction*, 3> m_rangeActions{};   // indexed by RangeMode
    std::array<QAction*, 3> m_coloursActions{}; // indexed by ColourMap
    FixedRangePopup* m_popup = nullptr;
};
