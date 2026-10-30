#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

#include <optional>
#include <vector>

class QImage;
class QScrollArea;
class QVBoxLayout;
class LiveChannelRow;

enum class LiveRowState { Active, Idle, Frozen };

// The Live group at the top of the sidebar: a row per live channel, in the order they started, each with the
// newest frame, its frame rate and its state. Hidden while there is no channel. The panel only shows: the
// window decides what a click or a close does.
class LiveChannelPanel : public QWidget {
    Q_OBJECT

public:
    static constexpr int kRowHeight = 40;  // compact; independent of the thumbnails' width-scaled cells
    static constexpr int kVisibleRows = 3; // more scroll

    explicit LiveChannelPanel(QWidget* parent = nullptr);

    void addChannel(const QString& name); // appended; shows the panel. Nothing if the name is already here
    // Called on the window's tick, not per frame. A frozen row's dot still shows whether frames arrive, which
    // the rate tells.
    void updateChannel(const QString& name, const QImage& thumbnail, int framesPerSecond, LiveRowState state);
    void removeChannel(const QString& name);              // hides the panel when the last goes
    void setSelected(const std::optional<QString>& name); // quiet: emits nothing; an unknown name clears
    std::optional<QString> selectedChannel() const;
    QStringList channelNames() const;

signals:
    void channelSelected(QString name); // on every click on a row, even the selected one
    void closeRequested(QString name);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    LiveChannelRow* rowOf(const QString& name) const;
    void fitHeight(); // as many rows as there are, up to kVisibleRows

    QScrollArea* m_scroll;
    QVBoxLayout* m_rowsLayout;
    std::vector<LiveChannelRow*> m_rows; // in the order they started
    std::optional<QString> m_selected;
};
