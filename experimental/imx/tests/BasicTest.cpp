#define LOG_TAG "BasicTest"
#include <log/log.h>

#include <gtest/gtest.h>

#include "GreyTestCommon.h"
#include "FinishJobTest.h"

namespace imx {

// A simple test which copies a single channel buffer from input to output.
TEST(BasicTest, CopyGrey) {
    // input must come from __input and output must be written to __output
    const char visa_string[] =
    "// Looping over variable simple.s0.y.__ipu_y aka y\n"
    "// Looping over variable simple.s0.x.__ipu_x aka x\n"
    "[test_stage]\n"
    "!visa\n"
    "input.b16 t1 <- __input[x*1+0][y*1+0][0];\n"
    "output.b16 __output[x*1+0][y*1+0][0] <- t1;\n"
    "terminate;";

    // Somewhat arbitrary test value.
    // Uses both high and low bytes, but with different values in each.
    const uint16_t test_value = 259;
    define_image_func expected_image =
        [test_value] (int, int) { return test_value; };

    // Somewhat arbitrary image sizes.  Big enough to be larger than one sheet,
    // but small enough to run quickly.  Different width and height to catch
    // any potential confusion between them.
    const int image_width = 27;
    const int image_height = 19;
    ASSERT_EQ(grey_test(image_width, image_height, visa_string,
                        expected_image, expected_image), 0);
}

TEST(BasicTest, FinishJob) {
  const int image_width = 4096;
  const int image_height = 3072;
  // Somewhat arbitrary test value.
  // Uses both high and low bytes, but with different values in each.
  const uint16_t test_value = 259;
  auto input_image = [test_value](int, int) { return test_value; };
  auto expected_image = [test_value](int, int) { return test_value + 1; };

  ASSERT_EQ(finish_job_test(image_width, image_height,
                            input_image, expected_image), 0);
}

}  // namespace imx

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
