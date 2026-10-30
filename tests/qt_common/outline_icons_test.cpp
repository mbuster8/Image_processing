#include "outline_icons.h"
#include "palette_support.h"

#include <QColor>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPalette>

#include <gtest/gtest.h>

#include <initializer_list>

namespace {

using qt_common::makeOutlineIcon;
using qt_common::OutlineIcon;

constexpr std::initializer_list<OutlineIcon> kAllShapes = {
    OutlineIcon::Pencil, OutlineIcon::Duplicate, OutlineIcon::Trash,     OutlineIcon::Pause,
    OutlineIcon::Play,   OutlineIcon::Stop,      OutlineIcon::ChevronUp, OutlineIcon::ChevronDown,
    OutlineIcon::Copy,   OutlineIcon::Clear,     OutlineIcon::Search,    OutlineIcon::Options,
    OutlineIcon::Info,   OutlineIcon::Warning,   OutlineIcon::Error,     OutlineIcon::Folder,
    OutlineIcon::Send,   OutlineIcon::Camera};

QImage render(OutlineIcon shape) { return makeOutlineIcon(shape).pixmap(24, 24).toImage(); }

int countDrawnPixels(const QImage& image) {
    int drawn = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            drawn += image.pixelColor(x, y).alpha() > 0 ? 1 : 0;
        }
    }
    return drawn;
}

// True when at least one pixel is fully opaque and every fully opaque pixel has the colour.
bool strokedIn(const QImage& image, const QColor& colour) {
    int opaque = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() == 255) {
                if (pixel.rgb() != colour.rgb()) {
                    return false;
                }
                ++opaque;
            }
        }
    }
    return opaque > 0;
}

void setWindowText(const QColor& colour) {
    QPalette palette = QGuiApplication::palette();
    palette.setColor(QPalette::WindowText, colour);
    QGuiApplication::setPalette(palette);
}

TEST(OutlineIcons, EveryShapeDrawsSomething) {
    for (const OutlineIcon shape : kAllShapes) {
        EXPECT_GT(countDrawnPixels(render(shape)), 10) << "shape " << static_cast<int>(shape);
    }
}

TEST(OutlineIcons, FollowTheApplicationPalette) {
    const test_support::PaletteGuard guard;

    setWindowText(Qt::red);
    EXPECT_TRUE(strokedIn(render(OutlineIcon::Pencil), Qt::red));

    setWindowText(Qt::blue);
    EXPECT_TRUE(strokedIn(render(OutlineIcon::Pencil), Qt::blue));
}

} // namespace
