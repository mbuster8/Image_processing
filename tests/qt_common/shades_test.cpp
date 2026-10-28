#include "palette_support.h"
#include "shades.h"

#include <QColor>
#include <QPalette>

#include <gtest/gtest.h>

#include <cstdlib>
#include <utility>

namespace {

using test_support::paletteWith;

bool isNeutral(const QColor& colour) {
    return colour.red() == colour.green() && colour.green() == colour.blue();
}

// Each shade steps further from the window colour towards the text, in a light and in a dark appearance, so
// a hovered cell differs from the panel, a selected one from a hovered one, and focus shows.
TEST(Shades, StepFromTheWindowColourTowardsTheText) {
    for (const auto& [window, text] : {std::pair{QColor(236, 236, 236), QColor(0, 0, 0)},
                                       std::pair{QColor(30, 30, 30), QColor(255, 255, 255)}}) {
        const QPalette palette = paletteWith(window, text);
        const auto away = [&](const QColor& shade) {
            return std::abs(shade.lightness() - window.lightness());
        };

        EXPECT_GT(away(qt_common::panelGrey(palette)), 0);
        EXPECT_GT(away(qt_common::hoverGrey(palette)), away(qt_common::panelGrey(palette)));
        EXPECT_GT(away(qt_common::selectionGrey(palette, false)), away(qt_common::hoverGrey(palette)));
        EXPECT_GT(away(qt_common::selectionGrey(palette, true)),
                  away(qt_common::selectionGrey(palette, false)));
        EXPECT_GT(away(qt_common::dividerGrey(palette)), away(qt_common::hoverGrey(palette)));
        for (const QColor& shade :
             {qt_common::panelGrey(palette), qt_common::hoverGrey(palette),
              qt_common::selectionGrey(palette, false), qt_common::dividerGrey(palette)}) {
            EXPECT_TRUE(isNeutral(shade)) << shade.name().toStdString();
        }
    }
}

} // namespace
