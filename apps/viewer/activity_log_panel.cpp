#include "activity_log_panel.h"

#include "activity_log.h"
#include "controls.h"
#include "outline_icons.h"

#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QListView>
#include <QScrollBar>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QToolButton* makeButton(const QString& objectName, const QString& text, qt_common::OutlineIcon icon,
                        const QString& tip) {
    auto* button = new QToolButton;
    button->setObjectName(objectName);
    button->setText(text);
    button->setIcon(qt_common::makeOutlineIcon(icon));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    qt_common::frameButton(button);
    button->setToolTip(tip);
    return button;
}

} // namespace

ActivityLogPanel::ActivityLogPanel(ActivityLog* log, QWidget* parent)
    : QWidget(parent), m_log(log), m_list(new QListView) {
    setObjectName(QStringLiteral("activityLogPanel"));
    m_list->setObjectName(QStringLiteral("logList"));
    m_list->setModel(m_log);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_list->setTextElideMode(Qt::ElideRight);

    auto* copy = makeButton(QStringLiteral("logCopy"), tr("Copy"), qt_common::OutlineIcon::Copy,
                            tr("Copy the selected entries, or all of them when none is selected"));
    auto* clear = makeButton(QStringLiteral("logClear"), tr("Clear"), qt_common::OutlineIcon::Clear,
                             tr("Empty the log, here and on disk"));
    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(style()->pixelMetric(QStyle::PM_LayoutLeftMargin), 0, 0, 0);
    buttons->addWidget(copy);
    buttons->addWidget(clear);
    buttons->addStretch(1);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(buttons);
    layout->addWidget(m_list, 1);

    m_list->installEventFilter(this);
    connect(copy, &QToolButton::clicked, this, &ActivityLogPanel::copySelection);
    connect(clear, &QToolButton::clicked, m_log, &ActivityLog::clear);
    connect(m_log, &QAbstractItemModel::rowsAboutToBeInserted, this, [this] {
        m_following = isAtTop();
        m_reading =
            m_following ? QPersistentModelIndex() : QPersistentModelIndex(m_list->indexAt(QPoint(0, 0)));
    });
    // A row inserted above the view would push what is being read down by a row.
    connect(m_log, &QAbstractItemModel::rowsInserted, this, [this] {
        if (m_following) {
            m_list->scrollToTop();
        } else if (m_reading.isValid()) {
            m_list->scrollTo(m_reading, QAbstractItemView::PositionAtTop);
        }
    });
}

// The copy key copies what the Copy button does: the list's own copy takes only the current entry. The
// window disables Copy Image while the panel has focus, so the key reaches the list.
bool ActivityLogPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_list && event->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent*>(event)->matches(QKeySequence::Copy)) {
        copySelection();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

// A hidden list has no valid scroll range, so it opens where it was following.
void ActivityLogPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (m_following) {
        m_list->scrollToTop();
    }
}

bool ActivityLogPanel::isAtTop() const {
    if (!m_list->isVisible()) {
        return m_following;
    }
    const QScrollBar* bar = m_list->verticalScrollBar();
    return bar->value() <= bar->minimum();
}

void ActivityLogPanel::copySelection() {
    QGuiApplication::clipboard()->setText(m_log->toPlainText(m_list->selectionModel()->selectedRows()));
}
