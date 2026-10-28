#include "thumbnail_panel.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>
#include <QVariant>

namespace {

viewer_core::ImageId idOf(const QModelIndex& index) {
    return index.data(ThumbnailModel::IdRole).value<quint64>();
}

} // namespace

ThumbnailPanel::ThumbnailPanel(QWidget* parent)
    : QListView(parent), m_model(new ThumbnailModel(this)), m_filter(new ThumbnailFilterModel(this)) {
    m_filter->setSourceModel(m_model);
    m_filter->setSort(SidebarSort::Received, naturalOrder(SidebarSort::Received));
    setModel(m_filter);
    auto* delegate = new ThumbnailDelegate(*m_model, this);
    setItemDelegate(delegate);
    connect(delegate, &ThumbnailDelegate::actionRequested, this, &ThumbnailPanel::actionRequested);
    // The hovered cell shows its icons.
    viewport()->setAttribute(Qt::WA_Hover);
    setViewMode(QListView::ListMode);
    setFlow(QListView::TopToBottom);
    setWrapping(false);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setUniformItemSizes(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current) {
        if (m_quiet > 0 || !current.isValid()) {
            return;
        }
        const viewer_core::ImageId id = idOf(current);
        if (m_selectedId != id) {
            m_selectedId = id;
            emit imageSelected(id);
        }
    });
}

void ThumbnailPanel::addImage(const viewer_core::ReceivedImage& image, bool select) {
    insertImage(image, 0, select);
}

void ThumbnailPanel::insertImage(const viewer_core::ReceivedImage& image, int historyIndex, bool select) {
    changeListing([&] { m_model->insertImage(image, historyIndex); });
    if (select) {
        selectImage(image.id);
    }
}

void ThumbnailPanel::removeImage(viewer_core::ImageId id) {
    changeListing([&] { m_model->removeImage(id); });
}

void ThumbnailPanel::updateImage(const viewer_core::ReceivedImage& image) {
    changeListing([&] { m_model->updateImage(image); });
}

bool ThumbnailPanel::selectImage(viewer_core::ImageId id) {
    const QModelIndex index = viewIndexOf(id);
    if (!index.isValid()) {
        m_selectedId = m_model->indexOf(id).isValid() ? std::optional(id) : std::nullopt;
        restoreSelection();
        return false;
    }
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    return true;
}

std::optional<viewer_core::ImageId> ThumbnailPanel::currentImageId() const {
    const QModelIndex current = currentIndex();
    return current.isValid() ? std::optional(idOf(current)) : std::nullopt;
}

bool ThumbnailPanel::isListed(viewer_core::ImageId id) const { return viewIndexOf(id).isValid(); }

std::vector<viewer_core::ImageId> ThumbnailPanel::listedIds() const {
    std::vector<viewer_core::ImageId> ids;
    ids.reserve(static_cast<std::size_t>(m_filter->rowCount()));
    for (int row = 0; row < m_filter->rowCount(); ++row) {
        ids.push_back(idOf(m_filter->index(row, 0)));
    }
    return ids;
}

int ThumbnailPanel::listedCount() const { return m_filter->rowCount(); }

int ThumbnailPanel::imageCount() const { return m_model->rowCount(); }

void ThumbnailPanel::setSearchText(const QString& text) {
    changeListing([&] { m_filter->setSearchText(text); });
}

void ThumbnailPanel::setSort(SidebarSort key, Qt::SortOrder order) {
    changeListing([&] { m_filter->setSort(key, order); });
}

void ThumbnailPanel::setShownFields(SidebarFields fields) {
    changeListing([&] { m_model->setShownFields(fields); });
    // Uniform item sizes keep the first cell's size until the items are laid out again. The proxy's re-sort
    // on the caption change happens to do that today; this does not depend on it.
    doItemsLayout();
}

SidebarFields ThumbnailPanel::shownFields() const { return m_model->shownFields(); }

Qt::SortOrder ThumbnailPanel::naturalOrder(SidebarSort key) {
    return key == SidebarSort::Name ? Qt::AscendingOrder : Qt::DescendingOrder;
}

QRect ThumbnailPanel::cellRect(viewer_core::ImageId id) const { return visualRect(viewIndexOf(id)); }

QRect ThumbnailPanel::thumbnailRect(viewer_core::ImageId id) const {
    const QModelIndex index = viewIndexOf(id);
    if (!index.isValid()) {
        return {};
    }
    return ThumbnailDelegate::pictureRect(visualRect(index), fontMetrics(), m_model->shownFields(),
                                          index.data(ThumbnailModel::ImageSizeRole).toSize());
}

QRect ThumbnailPanel::iconRect(viewer_core::ImageId id, ThumbnailAction action) const {
    const QModelIndex index = viewIndexOf(id);
    return index.isValid()
               ? ThumbnailDelegate::iconRect(visualRect(index), fontMetrics(), m_model->shownFields(), action)
               : QRect();
}

std::unique_ptr<QMenu> ThumbnailPanel::createContextMenu(viewer_core::ImageId id) {
    // No parent: the caller owns the menu, and a parent would delete it a second time.
    auto menu = std::make_unique<QMenu>();
    const auto add = [this, &menu, id](const QString& text, ThumbnailAction action) {
        connect(menu->addAction(text), &QAction::triggered, this,
                [this, id, action] { emit actionRequested(id, action); });
    };
    add(tr("Rename…"), ThumbnailAction::Rename);
    add(tr("Duplicate"), ThumbnailAction::Duplicate);
    add(tr("Copy Image"), ThumbnailAction::CopyImage);
    add(tr("Save As…"), ThumbnailAction::SaveAs);
    menu->addSeparator();
    add(tr("Delete"), ThumbnailAction::Delete);
    return menu;
}

void ThumbnailPanel::keyPressEvent(QKeyEvent* event) {
    // On a Mac keyboard the key labelled "delete" is Backspace.
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) &&
        event->modifiers() == Qt::NoModifier) {
        if (const auto id = currentImageId()) {
            emit actionRequested(*id, ThumbnailAction::Delete);
        }
        event->accept();
        return;
    }
    QListView::keyPressEvent(event);
}

void ThumbnailPanel::contextMenuEvent(QContextMenuEvent* event) {
    const bool fromKeyboard = event->reason() == QContextMenuEvent::Keyboard;
    const QModelIndex index = fromKeyboard ? currentIndex() : indexAt(event->pos());
    if (!index.isValid()) {
        return;
    }
    const QPoint position =
        fromKeyboard ? viewport()->mapToGlobal(visualRect(index).center()) : event->globalPos();
    createContextMenu(idOf(index))->exec(position);
}

// A list gaining focus with nothing current makes its first row current, which would show an image nobody
// chose: while the search hides the image in the view, for example.
void ThumbnailPanel::focusInEvent(QFocusEvent* event) {
    ++m_quiet;
    QListView::focusInEvent(event);
    --m_quiet;
    restoreSelection();
}

QModelIndex ThumbnailPanel::viewIndexOf(viewer_core::ImageId id) const {
    return m_filter->mapFromSource(m_model->indexOf(id));
}

void ThumbnailPanel::changeListing(const std::function<void()>& change) {
    ++m_quiet;
    change();
    --m_quiet;
    restoreSelection();
    emit listedChanged();
}

void ThumbnailPanel::restoreSelection() {
    if (m_selectedId && !m_model->indexOf(*m_selectedId).isValid()) {
        m_selectedId.reset(); // removed
    }
    const QModelIndex index = m_selectedId ? viewIndexOf(*m_selectedId) : QModelIndex();
    ++m_quiet;
    if (index.isValid()) {
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    } else {
        selectionModel()->clear();
    }
    --m_quiet;
}
