#pragma once

#include <QPersistentModelIndex>
#include <QWidget>

class ActivityLog;
class QListView;

// The log under the image: the entries, newest at the top, with Copy and Clear. It shows new entries as they
// arrive while scrolled to the top, and keeps the rows you are reading in place while scrolled down.
class ActivityLogPanel : public QWidget {
    Q_OBJECT

public:
    explicit ActivityLogPanel(ActivityLog* log, QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    bool isAtTop() const;
    void copySelection();

    ActivityLog* m_log;
    QListView* m_list;
    bool m_following = true;         // scrolled to the top when the last rows arrived
    QPersistentModelIndex m_reading; // otherwise the top row in view, kept there
};
