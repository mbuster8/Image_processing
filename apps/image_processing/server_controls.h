#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QToolButton;
class StateDot;

enum class ServerState { Starting, Listening, Paused, Stopped, NotListening };

// The server's buttons, state dot and status text, at the right of the status bar. The buttons are greyscale,
// as on an audio player; the dot is the only colour: green while listening, amber while paused, a hollow grey
// ring otherwise.
//
// | State        | Buttons       |
// |--------------|---------------|
// | Starting     | none          |
// | Listening    | Pause, Stop   |
// | Paused       | Resume, Stop  |
// | Stopped      | Start         |
// | NotListening | Try again     |
class ServerControls : public QWidget {
    Q_OBJECT

public:
    explicit ServerControls(QWidget* parent = nullptr);

    void setState(ServerState state, const QString& text);
    ServerState state() const;
    QString text() const;

signals:
    void pauseClicked(); // Pause, and Resume
    void stopClicked();  // Stop, and Start and Try again

private:
    QToolButton* m_pauseButton;
    QToolButton* m_stopButton;
    StateDot* m_dot;
    QLabel* m_text;
    ServerState m_state = ServerState::Starting;
};
