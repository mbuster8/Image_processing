#pragma once

#include <QSplitter>

class QSplitterHandle;

// A splitter whose handles show where to grab them: on the panel grey, a 1-point divider line along the
// middle with a short grip mark at its centre, in the divider grey (spec H3).
class DividerSplitter : public QSplitter {
public:
    static constexpr int kHandleWidth = 7;

    explicit DividerSplitter(Qt::Orientation orientation, QWidget* parent = nullptr);

protected:
    QSplitterHandle* createHandle() override;
};
