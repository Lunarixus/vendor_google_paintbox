#include "FinishJobTest.h"

#include <iostream>

#include "android-base/logging.h"

#include "Imx.h"

int finish_job_test(int in_width, int in_height,
                    define_image_func define_input_image,
                    define_image_func define_expected_output_image) {
  ImxError ret;
  const ImxNumericType img_numeric_type = IMX_UINT16;
  typedef uint16_t img_c_type; /* must correspond to IMX_UINT16 above */
  const int img_channel_size_bytes = sizeof(img_c_type);

  /* Setup */
  ImxDevice* device;
  ret = ImxGetDefaultDevice(&device);
  CHECK_EQ(IMX_SUCCESS, ret);

  ImxDeviceBufferHandle in_buffer_handle, out_buffer_handle;

  int img_size_bytes = in_width * in_height * img_channel_size_bytes;
  ret = ImxCreateDeviceBufferSimple(img_size_bytes, 0, &in_buffer_handle);
  CHECK_EQ(::IMX_SUCCESS, ret);
  ret = ImxCreateDeviceBufferSimple(img_size_bytes, 0, &out_buffer_handle);
  CHECK_EQ(::IMX_SUCCESS, ret);

  /* Fill input image */
  void *vaddr;
  ret = ImxLockDeviceBuffer(in_buffer_handle, &vaddr);
  CHECK_EQ(IMX_SUCCESS, ret);
  img_c_type *in_image = (img_c_type *) vaddr;
  int x, y;
  for (y = 0; y < in_height; ++y) {
    for (x = 0; x < in_width; ++x) {
      in_image[x + in_width * y] = define_input_image(x, y);
    }
  }
  ret = ImxUnlockDeviceBuffer(in_buffer_handle);
  CHECK_EQ(IMX_SUCCESS, ret);

  /* Fill output buffer with known junk value (999) so that we can later confirm
   * that it has been changed */
  ret = ImxLockDeviceBuffer(out_buffer_handle, &vaddr);
  CHECK_EQ(IMX_SUCCESS, ret);
  img_c_type *out_image = (img_c_type *) vaddr;
  for (y = 0; y < in_height; ++y) {
    for (x = 0; x < in_width; ++x) {
      out_image[x + in_width * (y)] = 999;
    }
  }
  ret = ImxUnlockDeviceBuffer(out_buffer_handle);
  CHECK_EQ(IMX_SUCCESS, ret);

  int out_width = 0, out_height = 0;
  ret = ImxExecuteFinishJob(in_buffer_handle, out_buffer_handle,
                            in_width, in_height, &out_width, &out_height);
  CHECK_EQ(IMX_SUCCESS, ret);
  CHECK_EQ(in_width, out_width);
  CHECK_EQ(in_height, out_height);

  /* Retrieve the result and verify that it matches what we expect */
  ret = ImxLockDeviceBuffer(out_buffer_handle, &vaddr);
  CHECK_EQ(::IMX_SUCCESS, ret);
  out_image = (img_c_type*) vaddr;
  for (y = 0; y < in_height; y++) {
    for (x = 0; x < in_width; x++) {
      img_c_type expected = define_expected_output_image(x, y);
      img_c_type actual = out_image[x + in_width * y];
      CHECK_EQ(actual, expected) << "< image mismatch at (" << x << "," << y
                                 << "): expected "
                                 << expected << ", got " << actual << "\n";
    }
  }
  ret = ImxUnlockDeviceBuffer(out_buffer_handle);
  CHECK_EQ(::IMX_SUCCESS, ret);

  /* Cleanup */
  ret = ImxDeleteDevice(device);
  CHECK_EQ(IMX_SUCCESS, ret);

  return 0;
}
