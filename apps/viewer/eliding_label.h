#pragma once

#include <QLabel>

// A label that can be narrower than its text, which it then elides on one line, so a long text never widens
// the window.
class ElidingLabel : public QLabel {
public:
    explicit ElidingLabel(Qt::TextElideMode mode, QWidget* parent = nullptr);

    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Qt::TextElideMode m_mode;
};
