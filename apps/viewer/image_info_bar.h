#pragma once

#include "display_text.h"

#include "viewer_core/image_history.h"

#include <QString>
#include <QWidget>

#include <optional>

class DisplaySettingsControl;
class QLabel;
class QLineEdit;
class QToolButton;

// One line above the image: its name, a pencil that renames it in place, its details, and at the right end
// the display range and colours. Hidden while no image is shown.
class ImageInfoBar : public QWidget {
    Q_OBJECT

public:
    explicit ImageInfoBar(QWidget* parent = nullptr);

    void showImage(viewer_core::ImageId id, const QString& name, const QString& details);
    // A live channel has no pencil: its name is the sender's. Ends a rename.
    void showLiveChannel(const QString& name, const QString& details);
    void clear();
    DisplaySettingsControl* displayControl() const;

    // Swaps the name for a text field, bound to the image shown now. Enter or leaving the field saves; Esc
    // cancels. Nothing while no image is shown.
    void startRename();
    bool isRenaming() const;
    std::optional<viewer_core::ImageId> renamingId() const;
    void cancelRename();

signals:
    // Only when the text changed. The caller trims and checks the name, and shows the result.
    void renameRequested(viewer_core::ImageId id, QString name);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void finishRename();
    void closeEditor();

    QLabel* m_name;
    QLineEdit* m_editor;
    QToolButton* m_renameButton;
    QLabel* m_details;
    DisplaySettingsControl* m_displayControl;
    std::optional<viewer_core::ImageId> m_shownId;
    QString m_shownName;
    std::optional<viewer_core::ImageId> m_renamingId;
    QString m_nameBeforeRename;
};
