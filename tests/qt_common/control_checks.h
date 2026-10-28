#pragma once

#include <QApplication>
#include <QFont>
#include <QSize>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

namespace test_support {

inline std::string nameOf(const QWidget& widget) { return widget.objectName().toStdString(); }

// A control large enough to see once laid out: at least 28 points high (spec H1).
inline testing::AssertionResult isTallEnough(const QWidget* control) {
    if (control == nullptr) {
        return testing::AssertionFailure() << "no such control";
    }
    if (control->height() < 28) {
        return testing::AssertionFailure() << nameOf(*control) << " is " << control->height() << " high";
    }
    return testing::AssertionSuccess();
}

// A button that shows its frame and an 18-point icon, at least 28 points high (spec H1). macOS draws a button
// anywhere inside a tool bar without its frame, whatever autoRaise says, and the tests' style cannot show it.
inline testing::AssertionResult isLargeFramedButton(const QToolButton* button) {
    if (const auto tall = isTallEnough(button); !tall) {
        return tall;
    }
    if (button->autoRaise()) {
        return testing::AssertionFailure() << nameOf(*button) << " hides its frame";
    }
    for (const QWidget* ancestor = button->parentWidget(); ancestor != nullptr;
         ancestor = ancestor->parentWidget()) {
        if (qobject_cast<const QToolBar*>(ancestor) != nullptr) {
            return testing::AssertionFailure()
                   << nameOf(*button) << " sits in a tool bar, which hides its frame";
        }
    }
    if (button->iconSize() != QSize(18, 18)) {
        return testing::AssertionFailure()
               << nameOf(*button) << " has " << button->iconSize().width() << "-point icons";
    }
    return testing::AssertionSuccess();
}

inline testing::AssertionResult usesTheRegularFont(const QWidget* widget) {
    if (widget == nullptr) {
        return testing::AssertionFailure() << "no such widget";
    }
    const qreal size = widget->font().pointSizeF();
    if (size != QApplication::font().pointSizeF()) {
        return testing::AssertionFailure() << nameOf(*widget) << " uses a " << size << "-point font";
    }
    return testing::AssertionSuccess();
}

// Gives status bars and tool buttons a smaller font than the application's, as macOS does, until the end of
// the test.
class SmallerClassFonts {
public:
    SmallerClassFonts() {
        QFont smaller = QApplication::font();
        smaller.setPointSizeF(std::max(1.0, smaller.pointSizeF() - 3));
        QApplication::setFont(smaller, "QStatusBar");
        QApplication::setFont(smaller, "QToolButton");
    }
    ~SmallerClassFonts() {
        QApplication::setFont(QApplication::font(), "QStatusBar");
        QApplication::setFont(QApplication::font(), "QToolButton");
    }
    SmallerClassFonts(const SmallerClassFonts&) = delete;
    SmallerClassFonts& operator=(const SmallerClassFonts&) = delete;
};

} // namespace test_support
