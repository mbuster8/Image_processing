#include <QApplication>
#include <QByteArray>

#include <gtest/gtest.h>

// Widgets need a QApplication. The offscreen platform lets these tests run without a display, from ctest,
// Qt Creator or a CI runner alike.
int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    testing::InitGoogleTest(&argc, argv);
    QApplication application(argc, argv);
    return RUN_ALL_TESTS();
}
