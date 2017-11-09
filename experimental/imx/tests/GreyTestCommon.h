#ifndef GREY_TEST_COMMON_H
#define GREY_TEST_COMMON_H

#include <stdint.h>
#include <functional>

namespace imx {

// header file for shared test infrastructure for single-channel 16-bit tests
using define_image_func = std::function<uint16_t(int x, int y)>;

// grey_test will setup a simple single channel copy test.
// img_width, img_height: Size of test image
// visa_string: vISA program to run
// define_input_image: a func(x,y) returning value of input image at that (x,y)
// define_expected_output_image:
//   a func(x,y) returning expected value of output image at that (x,y)
int grey_test(int img_width, int img_height,
              const char *visa_string,
              define_image_func define_input_image,
              define_image_func define_expected_output_image);

}  // namespace imx

#endif  // GREY_TEST_COMMON_H
