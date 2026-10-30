#include "display_settings_control.h"

#include <QAction>
#include <QActionGroup>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>

namespace {

using viewer_core::ColourMap;
using viewer_core::DisplayRange;
using viewer_core::RangeMode;

constexpr int kNumberPrecision = 10; // enough for a 32-bit integer and a float's digits

std::size_t indexOf(RangeMode mode) { return static_cast<std::size_t>(mode); }
std::size_t indexOf(ColourMap colours) { return static_cast<std::size_t>(colours); }

QString rangeName(RangeMode mode) {
    switch (mode) {
    case RangeMode::Hold:
        return DisplaySettingsControl::tr("Hold");
    case RangeMode::Fixed:
        return DisplaySettingsControl::tr("Fixed");
    case RangeMode::Auto:
        break;
    }
    return DisplaySettingsControl::tr("Auto");
}

QString coloursName(ColourMap colours) {
    switch (colours) {
    case ColourMap::Turbo:
        return QStringLiteral("Turbo");
    case ColourMap::Viridis:
        return QStringLiteral("Viridis");
    case ColourMap::Grey:
        break;
    }
    return DisplaySettingsControl::tr("Grey");
}

// Numbers with a full stop whatever the system's locale, so a range typed as in the pixel readout parses.
QString formatted(double value) { return QLocale::c().toString(value, 'g', kNumberPrecision); }

std::optional<double> parsed(const QString& text) {
    bool ok = false;
    const double value = QLocale::c().toDouble(text, &ok);
    return ok && std::isfinite(value) ? std::optional(value) : std::nullopt;
}

} // namespace

// Minimum and Maximum fields below the Range button. Apply is enabled only while the minimum is below the
// maximum; Enter applies, Esc and a click elsewhere close it unchanged.
class FixedRangePopup : public QFrame {
public:
    FixedRangePopup(QWidget* parent, std::function<void(DisplayRange)> apply)
        : QFrame(parent, Qt::Popup), m_minimum(new QLineEdit), m_maximum(new QLineEdit),
          m_apply(new QPushButton(DisplaySettingsControl::tr("Apply"))), m_onApply(std::move(apply)) {
        setObjectName(QStringLiteral("fixedRangePopup"));
        setFrameShape(QFrame::StyledPanel);
        m_minimum->setObjectName(QStringLiteral("fixedMinimum"));
        m_maximum->setObjectName(QStringLiteral("fixedMaximum"));
        m_apply->setObjectName(QStringLiteral("applyFixedRange"));
        m_apply->setDefault(true);
        for (QLineEdit* field : {m_minimum, m_maximum}) {
            auto* validator = new QDoubleValidator(field);
            validator->setLocale(QLocale::c());
            field->setValidator(validator);
            connect(field, &QLineEdit::textChanged, this, &FixedRangePopup::updateApply);
            connect(field, &QLineEdit::returnPressed, this, &FixedRangePopup::apply);
        }
        connect(m_apply, &QPushButton::clicked, this, &FixedRangePopup::apply);

        auto* fields = new QFormLayout;
        fields->addRow(DisplaySettingsControl::tr("Minimum"), m_minimum);
        fields->addRow(DisplaySettingsControl::tr("Maximum"), m_maximum);
        auto* buttons = new QHBoxLayout;
        buttons->addStretch(1);
        buttons->addWidget(m_apply);
        auto* layout = new QVBoxLayout(this);
        layout->addLayout(fields);
        layout->addLayout(buttons);
    }

    void open(const DisplayRange& range, const QPoint& globalPosition) {
        m_minimum->setText(formatted(range.min));
        m_maximum->setText(formatted(range.max));
        updateApply();
        move(globalPosition);
        show();
        m_minimum->setFocus();
        m_minimum->selectAll();
    }

protected:
    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Escape) {
            hide();
            return;
        }
        QFrame::keyPressEvent(event);
    }

private:
    std::optional<DisplayRange> typedRange() const {
        const std::optional<double> minimum = parsed(m_minimum->text());
        const std::optional<double> maximum = parsed(m_maximum->text());
        if (!minimum || !maximum || *minimum >= *maximum) {
            return std::nullopt;
        }
        return DisplayRange{*minimum, *maximum};
    }

    void updateApply() { m_apply->setEnabled(typedRange().has_value()); }

    void apply() {
        if (const std::optional<DisplayRange> range = typedRange()) {
            hide();
            m_onApply(*range);
        }
    }

    QLineEdit* m_minimum;
    QLineEdit* m_maximum;
    QPushButton* m_apply;
    std::function<void(DisplayRange)> m_onApply;
};

DisplaySettingsControl::DisplaySettingsControl(QWidget* parent)
    : QWidget(parent), m_rangeButton(new QToolButton), m_coloursButton(new QToolButton),
      m_rangeMenu(new QMenu(tr("Range"), this)), m_coloursMenu(new QMenu(tr("Colours"), this)) {
    auto* ranges = new QActionGroup(this);
    for (const RangeMode mode : {RangeMode::Auto, RangeMode::Hold, RangeMode::Fixed}) {
        QAction* action = ranges->addAction(mode == RangeMode::Fixed ? tr("Fixed…") : rangeName(mode));
        action->setCheckable(true);
        connect(action, &QAction::triggered, this, [this, mode] { chooseRange(mode); });
        m_rangeMenu->addAction(action);
        m_rangeActions[indexOf(mode)] = action;
    }
    m_rangeActions[indexOf(RangeMode::Hold)]->setToolTip(tr("Keep the current range for the frames to come"));
    auto* colourMaps = new QActionGroup(this);
    for (const ColourMap colours : {ColourMap::Grey, ColourMap::Turbo, ColourMap::Viridis}) {
        QAction* action = colourMaps->addAction(coloursName(colours));
        action->setCheckable(true);
        connect(action, &QAction::triggered, this, [this, colours] { chooseColours(colours); });
        m_coloursMenu->addAction(action);
        m_coloursActions[indexOf(colours)] = action;
    }

    m_rangeButton->setObjectName(QStringLiteral("rangeButton"));
    m_coloursButton->setObjectName(QStringLiteral("coloursButton"));
    // The ▾ is part of the text, and the menu pops up on a click, rather than given to the button: some
    // styles (Fusion among them) draw a menu button's arrow over the end of its text.
    for (const auto& [button, menu] :
         {std::pair{m_rangeButton, m_rangeMenu}, std::pair{m_coloursButton, m_coloursMenu}}) {
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        connect(button, &QToolButton::clicked, menu,
                [button, menu] { menu->popup(button->mapToGlobal(QPoint(0, button->height()))); });
    }
    m_coloursButton->setToolTip(tr("Colour map for single-channel images"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_rangeButton);
    layout->addWidget(m_coloursButton);
    showSettings();
}

void DisplaySettingsControl::setState(const viewer_core::DisplaySettings& settings, bool singleChannel) {
    if (m_popup != nullptr) {
        m_popup->hide(); // it was for the picture shown before
    }
    m_settings = settings;
    m_coloursButton->setVisible(singleChannel);
    for (QAction* action : m_coloursActions) {
        action->setEnabled(singleChannel);
    }
    showSettings();
}

void DisplaySettingsControl::setAutoRangeProvider(std::function<viewer_core::DisplayRange()> provider) {
    m_autoRange = std::move(provider);
}

QMenu* DisplaySettingsControl::rangeMenu() const { return m_rangeMenu; }

QMenu* DisplaySettingsControl::coloursMenu() const { return m_coloursMenu; }

void DisplaySettingsControl::chooseRange(RangeMode mode) {
    switch (mode) {
    case RangeMode::Auto:
        m_settings.mode = RangeMode::Auto;
        m_settings.range = {};
        break;
    case RangeMode::Hold:
        // The range Auto shows now, whatever the mode was.
        m_settings.mode = RangeMode::Hold;
        m_settings.range = m_autoRange ? m_autoRange() : m_settings.range;
        break;
    case RangeMode::Fixed:
        openFixedPopup();
        showSettings(); // the tick stays where it was until Apply
        return;
    }
    showSettings();
    emit settingsChanged(m_settings);
}

void DisplaySettingsControl::chooseColours(ColourMap colours) {
    m_settings.colours = colours;
    showSettings();
    emit settingsChanged(m_settings);
}

void DisplaySettingsControl::openFixedPopup() {
    if (m_popup == nullptr) {
        m_popup = new FixedRangePopup(this, [this](DisplayRange range) { applyFixed(range); });
    }
    m_popup->open(currentRange(), m_rangeButton->mapToGlobal(QPoint(0, m_rangeButton->height())));
}

void DisplaySettingsControl::applyFixed(DisplayRange range) {
    m_settings.mode = RangeMode::Fixed;
    m_settings.range = range;
    showSettings();
    emit settingsChanged(m_settings);
}

void DisplaySettingsControl::showSettings() {
    m_rangeButton->setText(tr("Range: %1 ▾").arg(rangeName(m_settings.mode)));
    m_rangeButton->setToolTip(
        m_settings.mode == RangeMode::Auto
            ? tr("Each picture from its own minimum to its maximum")
            : tr("%1 to %2").arg(formatted(m_settings.range.min), formatted(m_settings.range.max)));
    m_coloursButton->setText(tr("Colours: %1 ▾").arg(coloursName(m_settings.colours)));
    m_rangeActions[indexOf(m_settings.mode)]->setChecked(true);
    m_coloursActions[indexOf(m_settings.colours)]->setChecked(true);
}

DisplayRange DisplaySettingsControl::currentRange() const {
    if (m_settings.mode != RangeMode::Auto) {
        return m_settings.range;
    }
    return m_autoRange ? m_autoRange() : DisplayRange{};
}
