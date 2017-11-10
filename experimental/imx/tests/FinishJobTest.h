#ifndef FINISH_JOB_TEST_H
#define FINISH_JOB_TEST_H

#include <stdint.h>
#include <functional>

// header file for finish job test

typedef std::function<uint16_t(int x, int y)> define_image_func;

// img_width, img_height: Size of test image
// define_input_image: a func(x,y) returning value of input image at that (x,y)
// define_expected_output_image:
//   a func(x,y) returning expected value of output image at that (x,y)
int finish_job_test(int in_width, int in_height,
                    define_image_func define_input_image,
                    define_image_func define_expected_output_image);

#endif  // FINISH_JOB_TEST_H
