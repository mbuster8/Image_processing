#include "image_sidebar.h"

#include "controls.h"
#include "live_channel_panel.h"
#include "outline_icons.h"
#include "thumbnail_panel.h"

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSettings>
#include <QStringList>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cstddef>

namespace {

constexpr int kControlsMargin = 6;
constexpr int kControlsSpacing = 4;
const QString kShowKey = QStringLiteral("viewer/sidebarShow");
const QString kSortKey = QStringLiteral("viewer/sidebarSort");
const QString kSortDescendingKey = QStringLiteral("viewer/sidebarSortDescending");

// The fields in menu order, with their stored names.
constexpr std::array<SidebarField, 3> kFields{SidebarField::Thumbnail, SidebarField::Name,
                                              SidebarField::Details};
constexpr std::array<const char*, 3> kFieldNames{"thumbnail", "name", "details"};
// The stored names of the sort keys, in SidebarSort's order.
constexpr std::array<const char*, 4> kSortNames{"received", "modified", "name", "size"};

std::size_t indexOf(SidebarSort key) { return static_cast<std::size_t>(key); }

std::size_t indexOf(SidebarField field) {
    return static_cast<std::size_t>(std::find(kFields.begin(), kFields.end(), field) - kFields.begin());
}

} // namespace

ImageSidebar::ImageSidebar(QWidget* parent)
    : QWidget(parent), m_live(new LiveChannelPanel), m_thumbnails(new ThumbnailPanel),
      m_search(new QLineEdit), m_optionsButton(new QToolButton), m_count(new QLabel),
      m_optionsMenu(new QMenu(this)) {
    // A width range rather than a fixed width: the splitter sets the width, and a fixed one left an empty
    // strip wherever the splitter's section was wider than the sidebar.
    setMinimumWidth(kMinimumWidth);
    setMaximumWidth(kMaximumWidth);

    m_search->setObjectName(QStringLiteral("searchField"));
    m_search->setPlaceholderText(tr("Search names"));
    m_search->setClearButtonEnabled(true);
    qt_common::sizeControl(m_search);
    m_search->addAction(qt_common::makeOutlineIcon(qt_common::OutlineIcon::Search),
                        QLineEdit::LeadingPosition);
    m_search->installEventFilter(this);

    m_optionsButton->setObjectName(QStringLiteral("sidebarOptions"));
    m_optionsButton->setIcon(qt_common::makeOutlineIcon(qt_common::OutlineIcon::Options));
    m_optionsButton->setToolTip(tr("Show and sort"));
    qt_common::frameButton(m_optionsButton);
    m_optionsButton->setPopupMode(QToolButton::InstantPopup);
    m_optionsButton->setMenu(m_optionsMenu);

    m_count->setObjectName(QStringLiteral("searchCount"));
    m_count->setContentsMargins(kControlsMargin, 0, kControlsMargin, kControlsSpacing);
    m_count->hide();

    // Only the controls have margins; the list keeps the full width.
    auto* controls = new QHBoxLayout;
    controls->setContentsMargins(kControlsMargin, kControlsMargin, kControlsMargin, kControlsMargin);
    controls->setSpacing(kControlsSpacing);
    controls->addWidget(m_search, 1);
    controls->addWidget(m_optionsButton);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_live); // pinned above the search: a stream stays in reach however long the history
    layout->addLayout(controls);
    layout->addWidget(m_count);
    layout->addWidget(m_thumbnails, 1);

    const std::array<QString, 3> showTexts{tr("Thumbnail"), tr("Name"), tr("Details")};
    for (std::size_t i = 0; i < m_showActions.size(); ++i) {
        m_showActions[i] = new QAction(showTexts[i], this);
        m_showActions[i]->setCheckable(true);
        connect(m_showActions[i], &QAction::triggered, this, &ImageSidebar::applyShownFields);
    }

    const std::array<QString, 4> sortTexts{tr("Time Received"), tr("Time Modified"), tr("Name"), tr("Size")};
    auto* keys = new QActionGroup(this);
    for (std::size_t i = 0; i < m_sortActions.size(); ++i) {
        const auto key = static_cast<SidebarSort>(i);
        QAction* action = keys->addAction(sortTexts[i]);
        action->setCheckable(true);
        // A new key starts in its natural direction: newest first, A to Z, largest first.
        connect(action, &QAction::triggered, this, [this, key] {
            orderAction(ThumbnailPanel::naturalOrder(key))->setChecked(true);
            applySort();
        });
        m_sortActions[i] = action;
    }
    auto* orders = new QActionGroup(this);
    m_orderActions[Qt::AscendingOrder] = orders->addAction(tr("Ascending"));
    m_orderActions[Qt::DescendingOrder] = orders->addAction(tr("Descending"));
    for (QAction* action : m_orderActions) {
        action->setCheckable(true);
        connect(action, &QAction::triggered, this, &ImageSidebar::applySort);
    }
    addShowActions(m_optionsMenu->addMenu(tr("Show")));
    addSortActions(m_optionsMenu->addMenu(tr("Sort By")));
    loadShownFields();
    loadSort();

    connect(m_search, &QLineEdit::textChanged, m_thumbnails, &ThumbnailPanel::setSearchText);
    connect(m_search, &QLineEdit::textChanged, this, &ImageSidebar::updateCount);
    connect(m_thumbnails, &ThumbnailPanel::listedChanged, this, &ImageSidebar::updateCount);
}

LiveChannelPanel* ImageSidebar::liveChannels() const { return m_live; }

ThumbnailPanel* ImageSidebar::thumbnails() const { return m_thumbnails; }

QLineEdit* ImageSidebar::searchField() const { return m_search; }

QLabel* ImageSidebar::countLabel() const { return m_count; }

QMenu* ImageSidebar::optionsMenu() const { return m_optionsMenu; }

QAction* ImageSidebar::showAction(SidebarField field) const { return m_showActions[indexOf(field)]; }

QAction* ImageSidebar::sortAction(SidebarSort key) const { return m_sortActions[indexOf(key)]; }

QAction* ImageSidebar::orderAction(Qt::SortOrder order) const {
    return m_orderActions[static_cast<std::size_t>(order)];
}

void ImageSidebar::focusSearch() {
    m_search->setFocus(Qt::ShortcutFocusReason);
    m_search->selectAll();
}

void ImageSidebar::populateViewMenu(QMenu* viewMenu) {
    addShowActions(viewMenu->addMenu(tr("Show in Sidebar")));
    addSortActions(viewMenu->addMenu(tr("Sort By")));
}

// Esc clears the search, and in an empty field moves to the list. Enter and Down select the first listed
// image and move to the list.
bool ImageSidebar::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_search || event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }
    const auto* key = static_cast<QKeyEvent*>(event);
    switch (key->key()) {
    case Qt::Key_Escape:
        if (m_search->text().isEmpty()) {
            m_thumbnails->setFocus(Qt::OtherFocusReason);
        } else {
            m_search->clear();
        }
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Down:
        if (const auto listed = m_thumbnails->listedIds(); !listed.empty()) {
            m_thumbnails->selectImage(listed.front());
            m_thumbnails->setFocus(Qt::OtherFocusReason);
        }
        return true;
    default:
        return QWidget::eventFilter(watched, event);
    }
}

void ImageSidebar::addShowActions(QMenu* menu) const {
    for (QAction* action : m_showActions) {
        menu->addAction(action);
    }
}

void ImageSidebar::addSortActions(QMenu* menu) const {
    for (QAction* action : m_sortActions) {
        menu->addAction(action);
    }
    menu->addSeparator();
    for (QAction* action : m_orderActions) {
        menu->addAction(action);
    }
}

void ImageSidebar::loadShownFields() {
    // Unknown names are skipped; nothing left reads as the default, all three.
    SidebarFields fields;
    for (const QString& stored : QSettings().value(kShowKey).toStringList()) {
        for (std::size_t i = 0; i < kFieldNames.size(); ++i) {
            if (stored == QLatin1String(kFieldNames[i])) {
                fields |= kFields[i];
            }
        }
    }
    if (fields == SidebarFields()) {
        fields = SidebarField::Thumbnail | SidebarField::Name | SidebarField::Details;
    }
    for (std::size_t i = 0; i < kFields.size(); ++i) {
        m_showActions[i]->setChecked(fields.testFlag(kFields[i]));
    }
    applyShownFields();
}

// At least one field stays on: the last one ticked cannot be unticked.
void ImageSidebar::applyShownFields() {
    SidebarFields fields;
    QStringList names;
    for (std::size_t i = 0; i < kFields.size(); ++i) {
        if (m_showActions[i]->isChecked()) {
            fields |= kFields[i];
            names << QLatin1String(kFieldNames[i]);
        }
    }
    for (QAction* action : m_showActions) {
        action->setEnabled(!(action->isChecked() && names.size() == 1));
    }
    m_thumbnails->setShownFields(fields);
    QSettings().setValue(kShowKey, names);
}

void ImageSidebar::loadSort() {
    const QSettings settings;
    const QString stored = settings.value(kSortKey).toString();
    // Anything unknown reads as the default, Time Received.
    SidebarSort key = SidebarSort::Received;
    for (std::size_t i = 0; i < kSortNames.size(); ++i) {
        if (stored == QLatin1String(kSortNames[i])) {
            key = static_cast<SidebarSort>(i);
        }
    }
    const bool descending = settings.value(kSortDescendingKey, true).toBool();
    sortAction(key)->setChecked(true);
    orderAction(descending ? Qt::DescendingOrder : Qt::AscendingOrder)->setChecked(true);
    m_thumbnails->setSort(key, descending ? Qt::DescendingOrder : Qt::AscendingOrder);
}

void ImageSidebar::applySort() {
    SidebarSort key = SidebarSort::Received;
    for (std::size_t i = 0; i < m_sortActions.size(); ++i) {
        if (m_sortActions[i]->isChecked()) {
            key = static_cast<SidebarSort>(i);
        }
    }
    const Qt::SortOrder order =
        orderAction(Qt::DescendingOrder)->isChecked() ? Qt::DescendingOrder : Qt::AscendingOrder;
    m_thumbnails->setSort(key, order);
    QSettings settings;
    settings.setValue(kSortKey, QLatin1String(kSortNames[indexOf(key)]));
    settings.setValue(kSortDescendingKey, order == Qt::DescendingOrder);
}

void ImageSidebar::updateCount() {
    const bool searching = !m_search->text().isEmpty();
    if (searching) {
        m_count->setText(tr("%1 of %2").arg(m_thumbnails->listedCount()).arg(m_thumbnails->imageCount()));
    }
    m_count->setVisible(searching);
}
