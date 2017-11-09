#include "GreyTestCommon.h"

#include <iostream>

#include "android-base/logging.h"

#include "Imx.h"

namespace imx {

int grey_test(int img_width, int img_height, const char *visa_string,
              define_image_func define_input_image,
              define_image_func define_expected_output_image) {
    ImxError ret;

    const ImxNumericType img_numeric_type = ::IMX_UINT16;
    typedef uint16_t img_c_type; /* must correspond to ::IMX_UINT16 above */
    const int img_channel_size_bytes = sizeof(img_c_type);

    /* Setup */
    ImxDevice* device = nullptr;
    ret = ImxGetDefaultDevice(&device);
    CHECK_EQ(::IMX_SUCCESS, ret);

    /* Specify format and layout of input and output images by
     * creating transfer nodes */
    ImxCreateTransferNodeInfo tni = ImxDefaultCreateTransferNodeInfo();
    int dim = 2;
    tni.parameter_type.shape.dimensions = dim;
    for (int i=0; i < dim; i++) {
      tni.parameter_type.shape.dim[i].kind = ::IMX_ACTUAL_SIZE;
      tni.parameter_type.shape.dim[i].min = 0;
    }
    tni.parameter_type.shape.dim[0].extent = img_width;
    tni.parameter_type.shape.dim[1].extent = img_height;
    tni.parameter_type.element_type = img_numeric_type;
    tni.storage.element_type = img_numeric_type;
    tni.storage.layout = ::IMX_LAYOUT_LINEAR;
    tni.conversion = ::IMX_CONVERT_NONE;
    tni.border.mode = ::IMX_BORDER_ZERO;
    tni.use = ::IMX_MEMORY_READ;
    tni.stripe_width = 0;
    ImxNodeHandle dma_in;
    ret = ImxCreateTransferNode(&tni, &dma_in);
    CHECK_EQ(::IMX_SUCCESS, ret);
    tni.use = ::IMX_MEMORY_WRITE;
    ImxNodeHandle dma_out;
    ret = ImxCreateTransferNode(&tni, &dma_out);
    CHECK_EQ(::IMX_SUCCESS, ret);

    /* Create the graph from vISA string and the DMA transfer nodes */
    const int transfer_node_cnt = 2;
    ImxNodeHandle transfer_nodes[transfer_node_cnt];
    const char * transfer_node_names[transfer_node_cnt];
    transfer_nodes[0] = dma_in;
    transfer_node_names[0] = "__input";
    transfer_nodes[1] = dma_out;
    transfer_node_names[1] = "__output";
    ImxGraphHandle graph;
    ret = ImxCreateGraph(nullptr,
                         visa_string,
                         transfer_nodes,
                         transfer_node_names,
                         transfer_node_cnt,
                         &graph);
    CHECK_EQ(::IMX_SUCCESS, ret);

    /* Compile the graph */
    ImxCompileGraphInfo compile_info;
    compile_info.device = device;
    compile_info.params = nullptr;
    compile_info.num_params = 0;
    compile_info.options = nullptr;
    compile_info.num_options = 0;
    ImxCompiledGraphHandle compiled_graph;
    ret = ImxCompileGraph(graph, &compile_info, &compiled_graph);
    CHECK_EQ(::IMX_SUCCESS, ret);

    ret = ImxDeleteGraph(graph);
    CHECK_EQ(::IMX_SUCCESS, ret);
    graph = nullptr;

    /* Create job */
    ImxJobHandle job;
    ret = ImxCreateJob(compiled_graph, &job);
    CHECK_EQ(::IMX_SUCCESS, ret);

    /* Allocate buffers */
    /* TODO(billmark): Consider providing API routine to calculate size */
    int img_size_bytes = img_width * img_height * img_channel_size_bytes;
    ImxDeviceBufferHandle dma_in_buffer, dma_out_buffer;
    ret = ImxCreateDeviceBufferSimple(img_size_bytes, 0, &dma_in_buffer);
    CHECK_EQ(::IMX_SUCCESS, ret);
    ret = ImxCreateDeviceBufferSimple(img_size_bytes, 0, &dma_out_buffer);
    CHECK_EQ(::IMX_SUCCESS, ret);

    /* Provide buffer information to the job */
    const int buffer_cnt = 2;
    ImxFinalizeBufferInfo buffer_info[buffer_cnt];
    buffer_info[0].node = dma_in;
    buffer_info[0].config.buffer_type = ::IMX_DEVICE_BUFFER;
    buffer_info[0].config.buffer = dma_in_buffer;
    buffer_info[0].config.plane[0].offset = 0;
    buffer_info[0].config.plane[0].stride[0] = 0; /* Tightly packed */
    buffer_info[0].config.plane[0].stride[1] = 0; /* Tightly packed */
    buffer_info[0].config.plane[0].stride[2] = 0; /* Tightly packed */
    buffer_info[1].node = dma_out;
    buffer_info[1].config.buffer_type = ::IMX_DEVICE_BUFFER;
    buffer_info[1].config.buffer = dma_out_buffer;
    buffer_info[1].config.plane[0].offset = 0;
    buffer_info[1].config.plane[0].stride[0] = 0; /* Tightly packed */
    buffer_info[1].config.plane[0].stride[1] = 0; /* Tightly packed */
    buffer_info[1].config.plane[0].stride[2] = 0; /* Tightly packed */
    ret = ImxFinalizeBuffers(job, buffer_info, buffer_cnt);
    CHECK_EQ(::IMX_SUCCESS, ret);

    /* Fill input image */
    void *vaddr;
    ret = ImxLockDeviceBuffer(dma_in_buffer, &vaddr);
    CHECK_EQ(::IMX_SUCCESS, ret);
    img_c_type *in_image = (img_c_type *) vaddr;
    int x, y;
    for (y = 0; y < img_height; ++y) {
        for (x = 0; x < img_width; ++x) {
            in_image[x + img_width * y] = define_input_image(x, y);
        }
    }
    ret = ImxUnlockDeviceBuffer(dma_in_buffer);
    CHECK_EQ(::IMX_SUCCESS, ret);

    /* Fill output image with known junk value (999) so that we can later confirm
     * that it has been changed */
    ret = ImxLockDeviceBuffer(dma_out_buffer, &vaddr);
    CHECK_EQ(::IMX_SUCCESS, ret);
    img_c_type *out_image = (img_c_type *) vaddr;
    for (y = 0; y < img_height; ++y) {
        for (x = 0; x < img_width; ++x) {
            out_image[x + img_width * (y)] = 999;
        }
    }
    ret = ImxUnlockDeviceBuffer(dma_out_buffer);
    CHECK_EQ(::IMX_SUCCESS, ret);

    /* Invoke the IPU */
    ret = ImxExecuteJob(job);
    CHECK_EQ(::IMX_SUCCESS, ret);

    /* Retrieve the result and verify that it matches what we expect */
    ret = ImxLockDeviceBuffer(dma_out_buffer, &vaddr);
    CHECK_EQ(::IMX_SUCCESS, ret);
    out_image = (img_c_type *) vaddr;
    for (y = 0; y < img_height; y++) {
        for (x = 0; x < img_width; x++) {
            img_c_type expected = define_expected_output_image(x, y);
            img_c_type actual = out_image[x + img_width * y];
            if (actual != expected) {
                std::cout << "< image mismatch at (" << x << "," << y << "): expected "
                          << expected << ", got " << actual << "\n";
                exit(1);
            }
        }
    }
    ret = ImxUnlockDeviceBuffer(dma_out_buffer);
    CHECK_EQ(::IMX_SUCCESS, ret);

    /* Cleanup */
    ret = ImxDeleteDevice(device);
    CHECK_EQ(::IMX_SUCCESS, ret);
    return 0;
}

}  // namespace imx
