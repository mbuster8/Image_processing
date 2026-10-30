#include "image_info_bar.h"

#include "display_settings_control.h"
#include "display_text.h"
#include "eliding_label.h"
#include "outline_icons.h"
#include "thumbnail_action.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QStyle>
#include <QToolButton>

namespace {

constexpr int kVerticalMargin = 2;

} // namespace

ImageInfoBar::ImageInfoBar(QWidget* parent)
    : QWidget(parent), m_name(new ElidingLabel(Qt::ElideMiddle)), m_editor(new QLineEdit),
      m_renameButton(new QToolButton), m_details(new ElidingLabel(Qt::ElideRight)),
      m_displayControl(new DisplaySettingsControl), m_freezeButton(new QToolButton),
      m_snapshotButton(new QToolButton) {
    m_name->setObjectName(QStringLiteral("imageName"));
    m_editor->setObjectName(QStringLiteral("renameEditor"));
    m_renameButton->setObjectName(QStringLiteral("renameButton"));
    m_details->setObjectName(QStringLiteral("imageDetails"));

    m_editor->hide();
    m_editor->installEventFilter(this);
    m_renameButton->setIcon(qt_common::makeOutlineIcon(qt_common::OutlineIcon::Pencil));
    m_renameButton->setAutoRaise(true);
    m_renameButton->setToolTip(
        tr("Rename (%1)")
            .arg(thumbnailActionShortcut(ThumbnailAction::Rename).toString(QKeySequence::NativeText)));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(style()->pixelMetric(QStyle::PM_LayoutLeftMargin), kVerticalMargin,
                               style()->pixelMetric(QStyle::PM_LayoutRightMargin), kVerticalMargin);
    layout->addWidget(m_name);
    layout->addWidget(m_editor, 1); // takes the room while renaming; the stretch below takes it otherwise
    layout->addWidget(m_renameButton);
    layout->addWidget(m_details);
    layout->addStretch();
    // After the stretch, so they stay put while the text changes.
    layout->addWidget(m_displayControl);
    m_freezeButton->setObjectName(QStringLiteral("freezeButton"));
    m_snapshotButton->setObjectName(QStringLiteral("snapshotButton"));
    for (QToolButton* button : {m_freezeButton, m_snapshotButton}) {
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->hide();
        layout->addWidget(button);
    }

    connect(m_renameButton, &QToolButton::clicked, this, &ImageInfoBar::startRename);
    // Enter, and leaving the field, as in Finder.
    connect(m_editor, &QLineEdit::editingFinished, this, &ImageInfoBar::finishRename);

    hide();
}

void ImageInfoBar::showImage(viewer_core::ImageId id, const QString& name, const QString& details) {
    m_shownId = id;
    m_shownName = name;
    m_name->setText(singleLine(name));
    m_name->setToolTip(name);
    m_renameButton->setVisible(!isRenaming());
    m_details->setText(details);
    m_details->setToolTip(QString());
    m_freezeButton->hide();
    m_snapshotButton->hide();
    show();
}

void ImageInfoBar::showLiveChannel(const QString& name, const QString& details) {
    cancelRename();
    m_shownId.reset();
    m_shownName = name;
    m_name->setText(singleLine(name));
    m_name->setToolTip(name);
    m_renameButton->hide();
    m_details->setText(details);
    m_details->setToolTip(tr("Frames arrived and drawn in the last second, frames dropped while the viewer "
                             "was busy, and the frame's age: the time it arrived minus the time the sender "
                             "stamped on it, which means something only when both run on one computer."));
    m_freezeButton->setVisible(m_freezeButton->defaultAction() != nullptr);
    m_snapshotButton->setVisible(m_snapshotButton->defaultAction() != nullptr);
    show();
}

void ImageInfoBar::clear() {
    cancelRename();
    m_shownId.reset();
    m_shownName.clear();
    hide();
}

DisplaySettingsControl* ImageInfoBar::displayControl() const { return m_displayControl; }

void ImageInfoBar::setLiveActions(QAction* freeze, QAction* snapshot) {
    m_freezeButton->setDefaultAction(freeze);
    m_snapshotButton->setDefaultAction(snapshot);
}

void ImageInfoBar::startRename() {
    if (!m_shownId) {
        return;
    }
    if (!m_renamingId) {
        m_renamingId = m_shownId;
        m_nameBeforeRename = m_shownName;
        m_editor->setText(m_shownName);
        m_name->hide();
        m_renameButton->hide();
        m_editor->show();
    }
    m_editor->setFocus();
    m_editor->selectAll();
}

bool ImageInfoBar::isRenaming() const { return m_renamingId.has_value(); }

std::optional<viewer_core::ImageId> ImageInfoBar::renamingId() const { return m_renamingId; }

void ImageInfoBar::cancelRename() { closeEditor(); }

bool ImageInfoBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_editor && event->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        cancelRename();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void ImageInfoBar::finishRename() {
    if (!m_renamingId) {
        return;
    }
    const viewer_core::ImageId id = *m_renamingId;
    const QString name = m_editor->text();
    closeEditor();
    if (name != m_nameBeforeRename) {
        emit renameRequested(id, name);
    }
}

// Ends the rename before hiding the field: hiding it takes its focus away, which reports the edit as finished
// a second time.
void ImageInfoBar::closeEditor() {
    if (!m_renamingId) {
        return;
    }
    m_renamingId.reset();
    m_editor->hide();
    m_name->show();
    m_renameButton->show();
}
