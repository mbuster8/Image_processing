#include "live_channel_panel.h"

#include "eliding_label.h"
#include "outline_icons.h"
#include "shades.h"

#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kMargin = 6; // the sidebar controls' margin
constexpr int kSpacing = 4;
constexpr int kPadding = 4;
constexpr int kThumbnailHeight = LiveChannelPanel::kRowHeight - 2 * kPadding;
constexpr int kThumbnailWidth = kThumbnailHeight * 3 / 2; // room for 4:3 and 16:9 pictures alike
constexpr int kDotSize = 8;
constexpr int kCloseSize = 20;
constexpr int kCloseIconSize = 14;

// A filled circle in the accent colour while frames arrive and grey otherwise, read at paint time so a change
// of appearance is followed. Never colour alone: the state's word stands beside it.
class StatusDot : public QWidget {
public:
    explicit StatusDot(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName(QStringLiteral("liveDot"));
        setFixedSize(kDotSize, kDotSize);
    }

    void setActive(bool active) {
        if (active != m_active) {
            m_active = active;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        const QPalette palette = QGuiApplication::palette();
        painter.setBrush(m_active ? qt_common::accentColour(palette)
                                  : palette.color(QPalette::PlaceholderText));
        painter.drawEllipse(rect());
    }

private:
    bool m_active = true;
};

QString stateText(LiveRowState state) {
    switch (state) {
    case LiveRowState::Idle:
        return LiveChannelPanel::tr("idle");
    case LiveRowState::Frozen:
        return LiveChannelPanel::tr("frozen");
    case LiveRowState::Active:
        break;
    }
    return LiveChannelPanel::tr("live");
}

} // namespace

// One channel: its newest frame, its name, and below the name its rate and state. Painted like a thumbnail
// cell: the selection and hover greys, and a divider line underneath.
class LiveChannelRow : public QWidget {
public:
    explicit LiveChannelRow(const QString& name)
        : m_name(name), m_thumbnail(new QLabel), m_rate(new QLabel), m_dot(new StatusDot),
          m_state(new QLabel), m_close(new QToolButton) {
        setObjectName(QStringLiteral("liveRow_") + name);
        setAccessibleName(name);
        setFixedHeight(LiveChannelPanel::kRowHeight);
        setAttribute(Qt::WA_Hover); // repainted as the pointer comes and goes

        m_thumbnail->setObjectName(QStringLiteral("liveThumbnail"));
        m_thumbnail->setFixedSize(kThumbnailWidth, kThumbnailHeight);
        m_thumbnail->setAlignment(Qt::AlignCenter);
        auto* title = new ElidingLabel(Qt::ElideRight);
        title->setObjectName(QStringLiteral("liveName"));
        title->setText(name);
        m_rate->setObjectName(QStringLiteral("liveRate"));
        m_state->setObjectName(QStringLiteral("liveState"));
        m_state->setText(stateText(LiveRowState::Active));

        m_close->setObjectName(QStringLiteral("liveClose"));
        m_close->setIcon(qt_common::makeOutlineIcon(qt_common::OutlineIcon::Clear));
        m_close->setIconSize(QSize(kCloseIconSize, kCloseIconSize));
        m_close->setFixedSize(kCloseSize, kCloseSize);
        m_close->setAutoRaise(true);
        m_close->setToolTip(LiveChannelPanel::tr("Close this channel"));
        m_close->setAccessibleName(LiveChannelPanel::tr("Close %1").arg(name));
        m_close->hide();

        auto* status = new QHBoxLayout;
        status->setContentsMargins(0, 0, 0, 0);
        status->setSpacing(kSpacing);
        status->addWidget(m_rate);
        status->addWidget(m_dot, 0, Qt::AlignVCenter);
        status->addWidget(m_state);
        status->addStretch(1);
        auto* text = new QVBoxLayout;
        text->setContentsMargins(0, 0, 0, 0);
        text->setSpacing(0);
        text->addWidget(title);
        text->addLayout(status);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(kMargin, kPadding, kMargin, kPadding);
        layout->setSpacing(kSpacing);
        layout->addWidget(m_thumbnail);
        layout->addLayout(text, 1);
        layout->addWidget(m_close, 0, Qt::AlignVCenter);
    }

    const QString& name() const { return m_name; }
    QToolButton* closeButton() const { return m_close; }

    void showState(const QImage& thumbnail, int framesPerSecond, LiveRowState state) {
        if (thumbnail.isNull()) {
            m_thumbnail->clear();
        } else {
            const qreal ratio = m_thumbnail->devicePixelRatioF();
            QPixmap pixmap = QPixmap::fromImage(
                thumbnail.scaled(m_thumbnail->size() * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            pixmap.setDevicePixelRatio(ratio);
            m_thumbnail->setPixmap(pixmap);
        }
        m_rate->setText(LiveChannelPanel::tr("%1 fps").arg(framesPerSecond));
        m_state->setText(stateText(state));
        m_dot->setActive(state == LiveRowState::Active ||
                         (state == LiveRowState::Frozen && framesPerSecond > 0));
        m_close->setVisible(state == LiveRowState::Idle);
    }

    void setSelected(bool selected) {
        if (selected != m_selected) {
            m_selected = selected;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        // Rows never take focus, so the selection is the unfocused grey.
        const QPalette palette = QGuiApplication::palette();
        if (m_selected) {
            painter.fillRect(rect(), qt_common::selectionGrey(palette, false));
        } else if (underMouse()) {
            painter.fillRect(rect(), qt_common::hoverGrey(palette));
        }
        painter.fillRect(0, height() - 1, width(), 1, qt_common::dividerGrey(palette));
    }

private:
    QString m_name;
    QLabel* m_thumbnail;
    QLabel* m_rate;
    StatusDot* m_dot;
    QLabel* m_state;
    QToolButton* m_close;
    bool m_selected = false;
};

LiveChannelPanel::LiveChannelPanel(QWidget* parent)
    : QWidget(parent), m_scroll(new QScrollArea), m_rowsLayout(new QVBoxLayout) {
    auto* heading = new QLabel(tr("Live"));
    heading->setObjectName(QStringLiteral("liveHeading"));
    heading->setContentsMargins(kMargin, kMargin, kMargin, kPadding);
    QFont bold = heading->font();
    bold.setBold(true);
    heading->setFont(bold);

    auto* rows = new QWidget;
    rows->setLayout(m_rowsLayout);
    m_rowsLayout->setContentsMargins(0, 0, 0, 0);
    m_rowsLayout->setSpacing(0);
    m_rowsLayout->addStretch(1); // rows go above it
    m_scroll->setWidget(rows);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(heading);
    layout->addWidget(m_scroll);
    hide();
}

void LiveChannelPanel::addChannel(const QString& name) {
    if (rowOf(name) != nullptr) {
        return;
    }
    auto* row = new LiveChannelRow(name);
    m_rowsLayout->insertWidget(static_cast<int>(m_rows.size()), row);
    row->installEventFilter(this);
    connect(row->closeButton(), &QToolButton::clicked, this, [this, name] { emit closeRequested(name); });
    m_rows.push_back(row);
    fitHeight();
    show();
}

void LiveChannelPanel::updateChannel(const QString& name, const QImage& thumbnail, int framesPerSecond,
                                     LiveRowState state) {
    if (LiveChannelRow* row = rowOf(name)) {
        row->showState(thumbnail, framesPerSecond, state);
    }
}

void LiveChannelPanel::removeChannel(const QString& name) {
    LiveChannelRow* row = rowOf(name);
    if (row == nullptr) {
        return;
    }
    m_rows.erase(std::find(m_rows.begin(), m_rows.end(), row));
    if (m_selected == name) {
        m_selected.reset();
    }
    // Later, not now: the request may come from the row's own close button, still inside its click.
    m_rowsLayout->removeWidget(row);
    row->hide();
    row->setParent(nullptr);
    row->deleteLater();
    if (m_rows.empty()) {
        hide();
    } else {
        fitHeight();
    }
}

void LiveChannelPanel::setSelected(const std::optional<QString>& name) {
    m_selected = name && rowOf(*name) != nullptr ? name : std::nullopt;
    for (LiveChannelRow* row : m_rows) {
        row->setSelected(m_selected == row->name());
    }
}

std::optional<QString> LiveChannelPanel::selectedChannel() const { return m_selected; }

QStringList LiveChannelPanel::channelNames() const {
    QStringList names;
    for (const LiveChannelRow* row : m_rows) {
        names << row->name();
    }
    return names;
}

// A press anywhere on a row but its close button chooses the channel: the labels pass presses on to the row.
bool LiveChannelPanel::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress &&
        static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        const auto found = std::find(m_rows.begin(), m_rows.end(), watched);
        if (found != m_rows.end()) {
            const QString name = (*found)->name();
            setSelected(name);
            emit channelSelected(name);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

LiveChannelRow* LiveChannelPanel::rowOf(const QString& name) const {
    const auto found = std::find_if(m_rows.begin(), m_rows.end(),
                                    [&name](const LiveChannelRow* row) { return row->name() == name; });
    return found != m_rows.end() ? *found : nullptr;
}

void LiveChannelPanel::fitHeight() {
    const auto shown = std::min<std::size_t>(m_rows.size(), kVisibleRows);
    m_scroll->setFixedHeight(static_cast<int>(shown) * kRowHeight);
}
