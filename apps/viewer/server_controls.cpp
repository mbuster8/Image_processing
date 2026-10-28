#include "server_controls.h"

#include "controls.h"
#include "outline_icons.h"

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QToolButton>

namespace {

constexpr int kDotSize = 12;
constexpr qreal kRingWidth = 1.5;
constexpr int kSpacing = 4;
const QColor kListeningColour(0x2e, 0x9e, 0x4f);
const QColor kPausedColour(0xd9, 0x9a, 0x00);

QToolButton* makeButton(const QString& objectName) {
    auto* button = new QToolButton;
    button->setObjectName(objectName);
    qt_common::frameButton(button);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    return button;
}

// Text, icon and tooltip together: the text is what the tooltip and accessibility tools read.
void setButton(QToolButton* button, qt_common::OutlineIcon icon, const QString& text) {
    button->setIcon(qt_common::makeOutlineIcon(icon));
    button->setText(text);
    button->setToolTip(text);
}

} // namespace

// Filled for a running server, a ring for one that is not: the ring reads as "off" without a colour.
class StateDot : public QWidget {
public:
    StateDot() {
        setObjectName(QStringLiteral("stateDot"));
        setFixedSize(kDotSize, kDotSize);
    }

    void setState(ServerState state) {
        m_state = state;
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF area =
            QRectF(rect()).adjusted(kRingWidth / 2, kRingWidth / 2, -kRingWidth / 2, -kRingWidth / 2);
        switch (m_state) {
        case ServerState::Listening:
        case ServerState::Paused:
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_state == ServerState::Listening ? kListeningColour : kPausedColour);
            break;
        case ServerState::Starting:
        case ServerState::Stopped:
        case ServerState::NotListening:
            // Read from the palette at paint time, so the ring follows a change of appearance.
            painter.setPen(QPen(palette().color(QPalette::Mid), kRingWidth));
            painter.setBrush(Qt::NoBrush);
            break;
        }
        painter.drawEllipse(area);
    }

private:
    ServerState m_state = ServerState::Starting;
};

ServerControls::ServerControls(QWidget* parent)
    : QWidget(parent), m_pauseButton(makeButton(QStringLiteral("pauseButton"))),
      m_stopButton(makeButton(QStringLiteral("stopButton"))), m_dot(new StateDot), m_text(new QLabel) {
    m_text->setObjectName(QStringLiteral("serverStatus"));
    m_text->setTextInteractionFlags(Qt::TextSelectableByMouse); // the endpoint can be copied
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kSpacing);
    layout->addWidget(m_pauseButton);
    layout->addWidget(m_stopButton);
    layout->addWidget(m_dot);
    layout->addWidget(m_text);
    connect(m_pauseButton, &QToolButton::clicked, this, &ServerControls::pauseClicked);
    connect(m_stopButton, &QToolButton::clicked, this, &ServerControls::stopClicked);
    setState(ServerState::Starting, QString());
}

void ServerControls::setState(ServerState state, const QString& text) {
    m_state = state;
    m_text->setText(text);
    m_dot->setState(state);
    switch (state) {
    case ServerState::Starting:
        break;
    case ServerState::Listening:
        setButton(m_pauseButton, qt_common::OutlineIcon::Pause, tr("Pause"));
        setButton(m_stopButton, qt_common::OutlineIcon::Stop, tr("Stop"));
        break;
    case ServerState::Paused:
        setButton(m_pauseButton, qt_common::OutlineIcon::Play, tr("Resume"));
        setButton(m_stopButton, qt_common::OutlineIcon::Stop, tr("Stop"));
        break;
    case ServerState::Stopped:
        setButton(m_stopButton, qt_common::OutlineIcon::Play, tr("Start"));
        break;
    case ServerState::NotListening:
        setButton(m_stopButton, qt_common::OutlineIcon::Play, tr("Try again"));
        break;
    }
    m_pauseButton->setVisible(state == ServerState::Listening || state == ServerState::Paused);
    m_stopButton->setVisible(state != ServerState::Starting);
}

ServerState ServerControls::state() const { return m_state; }

QString ServerControls::text() const { return m_text->text(); }
