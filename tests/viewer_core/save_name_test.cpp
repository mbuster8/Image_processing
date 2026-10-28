#include "viewer_core/save_name.h"

#include <gtest/gtest.h>

#include <string>

namespace {

using viewer_core::saveNameStem;

TEST(SaveName, DropsImageExtension) { EXPECT_EQ(saveNameStem("photo.webp"), "photo"); }

TEST(SaveName, DropsExtensionBeforeSendNumber) { EXPECT_EQ(saveNameStem("photo.webp #3"), "photo #3"); }

TEST(SaveName, KeepsDotThatIsNotAnImageExtension) { EXPECT_EQ(saveNameStem("result v1.2"), "result v1.2"); }

TEST(SaveName, IgnoresExtensionCase) { EXPECT_EQ(saveNameStem("A.PNG"), "A"); }

TEST(SaveName, KeepsNameThatIsOnlyAnExtension) { EXPECT_EQ(saveNameStem(".png"), ".png"); }

TEST(SaveName, KeepsNameWithoutExtension) { EXPECT_EQ(saveNameStem("Sobel X"), "Sobel X"); }

} // namespace
