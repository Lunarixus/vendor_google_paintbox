#include "ImxProtoConversions.h"

namespace imx {

// ConvertStatus
::imx::ImxError ConvertStatus(::ImxError result) {
    switch (result) {
        case ::IMX_SUCCESS: return ::imx::IMX_SUCCESS;
        default: return ::imx::IMX_FAILURE;
    }
}

::ImxError ConvertStatus(::imx::ImxError result) {
    switch (result) {
        case ::imx::IMX_SUCCESS: return ::IMX_SUCCESS;
        default: return ::IMX_FAILURE;
    }
}

// ConvertParameterUse
::imx::ParameterUse ConvertParameterUse(::ImxParameterUse in) {
    switch (in) {
        case IMX_MEMORY_READ: return MEMORY_READ;
        case IMX_MEMORY_WRITE: return MEMORY_WRITE;
        case IMX_MIPI_READ: return MIPI_READ;
        case IMX_MIPI_WRITE: return MIPI_WRITE;
        case IMX_MIPI_READ_MEMORY_WRITE: return MIPI_READ_MEMORY_WRITE;
    }
}

::ImxParameterUse ConvertParameterUse(::imx::ParameterUse in) {
    switch (in) {
        case MEMORY_READ: return IMX_MEMORY_READ;
        case MEMORY_WRITE: return IMX_MEMORY_WRITE;
        case MIPI_READ: return IMX_MIPI_READ;
        case MIPI_WRITE: return IMX_MIPI_WRITE;
        case MIPI_READ_MEMORY_WRITE: return IMX_MIPI_READ_MEMORY_WRITE;
    }
}

// ConvertNumericType
::imx::NumericType ConvertNumericType(::ImxNumericType in) {
    switch (in) {
        case IMX_INT8: return INT8;
        case IMX_UINT8: return UINT8;
        case IMX_INT16: return INT16;
        case IMX_UINT16: return UINT16;
        case IMX_INT32: return INT32;
        case IMX_UINT32: return UINT32;
        case IMX_FLOAT16: return FLOAT16;
        case IMX_FLOAT32: return FLOAT32;
        case IMX_UINT10: return UINT10;
        case IMX_UINT12: return UINT12;
        case IMX_PACKED_UINT_6_5_6: return PACKED_UINT_6_5_6;
        case IMX_PACKED_UINT_5_5_5_1: return PACKED_UINT_5_5_5_1;
        case IMX_PACKED_UINT_1_5_5_5: return PACKED_UINT_1_5_5_5;
        case IMX_PACKED_UINT_2_10_10_10: return PACKED_UINT_2_10_10_10;
        case IMX_PACKED_UINT_10_10_10_2: return PACKED_UINT_10_10_10_2;
        default: return INT8; // TODO(joshualitt) log or something?
    }
}

::ImxNumericType ConvertNumericType(::imx::NumericType in) {
    switch (in) {
        case INT8: return IMX_INT8;
        case UINT8: return IMX_UINT8;
        case INT16: return IMX_INT16;
        case UINT16: return IMX_UINT16;
        case INT32: return IMX_INT32;
        case UINT32: return IMX_UINT32;
        case FLOAT16: return IMX_FLOAT16;
        case FLOAT32: return IMX_FLOAT32;
        case UINT10: return IMX_UINT10;
        case UINT12: return IMX_UINT12;
        case PACKED_UINT_6_5_6: return IMX_PACKED_UINT_6_5_6;
        case PACKED_UINT_5_5_5_1: return IMX_PACKED_UINT_5_5_5_1;
        case PACKED_UINT_1_5_5_5: return IMX_PACKED_UINT_1_5_5_5;
        case PACKED_UINT_2_10_10_10: return IMX_PACKED_UINT_2_10_10_10;
        case PACKED_UINT_10_10_10_2: return IMX_PACKED_UINT_10_10_10_2;
    }
}

// ConvertSizeKind
::imx::SizeKind ConvertSizeKind(::ImxSizeKind in) {
    switch (in) {
        case IMX_ACTUAL_SIZE: return imx::ACTUAL_SIZE;
        case IMX_MAX_SIZE: return imx::MAX_SIZE;
        case IMX_UNKNOWN_SIZE: return imx::UNKNOWN_SIZE;
        default: return imx::ACTUAL_SIZE; // TODO(joshualitt) error case
    }
}

::ImxSizeKind ConvertSizeKind(::imx::SizeKind in) {
    switch (in) {
        case imx::ACTUAL_SIZE: return IMX_ACTUAL_SIZE;
        case imx::MAX_SIZE: return IMX_MAX_SIZE;
        case imx::UNKNOWN_SIZE: return IMX_UNKNOWN_SIZE;
    }
}

// ConvertShape
void ConvertShape(const ::ImxShape& in, ::imx::Shape* out) {
    out->set_dimensions(in.dimensions);
    for (unsigned int d = 0; d < in.dimensions; d++) {
        auto* dim = out->add_dim();
        dim->set_kind(ConvertSizeKind(in.dim[d].kind));
        dim->set_extent(in.dim[d].extent);
        dim->set_min(in.dim[d].min);
    }
}

void ConvertShape(const ::imx::Shape& in, ::ImxShape* out) {
    out->dimensions = in.dimensions();
    for (unsigned int d = 0; d < in.dimensions(); d++) {
        out->dim[d].kind = ConvertSizeKind(in.dim(d).kind());
        out->dim[d].extent = in.dim(d).extent();
        out->dim[d].min = in.dim(d).min();
    }
}

// ConvertParameterType
void ConvertParameterType(
    const ::ImxParameterType& in, ::imx::ParameterType* out) {
    ConvertShape(in.shape, out->mutable_shape());
    out->set_element_type(ConvertNumericType(in.element_type));
}

void ConvertParameterType(
    const ::imx::ParameterType& in, ::ImxParameterType* out) {
    ConvertShape(in.shape(), &out->shape);
    out->element_type = ConvertNumericType(in.element_type());
}

// ConvertLayout
::imx::Layout ConvertLayout(::ImxLayout in) {
    switch (in) {
        case IMX_LAYOUT_LINEAR: return ::imx::LINEAR;
        case IMX_LAYOUT_PLANAR: return ::imx::PLANAR;
        case IMX_LAYOUT_LINEAR_PLANAR: return ::imx::LINEAR_PLANAR;
        case IMX_LAYOUT_RASTER_RAW10: return ::imx::RASTER_RAW10;
        case IMX_LAYOUT_LINEAR_TILED_4X4: return ::imx::LINEAR_TILED_4X4;
        case IMX_LAYOUT_PLANAR_TILED_4X4: return ::imx::PLANAR_TILED_4X4;
        default: return ::imx::LINEAR;  // TODO
    }
}

::ImxLayout ConvertLayout(::imx::Layout in) {
    switch (in) {
        case ::imx::LINEAR: return IMX_LAYOUT_LINEAR;
        case ::imx::PLANAR: return IMX_LAYOUT_PLANAR;
        case ::imx::LINEAR_PLANAR: return IMX_LAYOUT_LINEAR_PLANAR;
        case ::imx::RASTER_RAW10: return IMX_LAYOUT_RASTER_RAW10;
        case ::imx::LINEAR_TILED_4X4: return IMX_LAYOUT_LINEAR_TILED_4X4;
        case ::imx::PLANAR_TILED_4X4: return IMX_LAYOUT_PLANAR_TILED_4X4;
    }
}

// ConvertStorage
void ConvertStorage(const ::ImxStorage& in, ::imx::Storage* out) {
    out->set_element_type(ConvertNumericType(in.element_type));
    out->set_layout(ConvertLayout(in.layout));
}

void ConvertStorage(const ::imx::Storage& in, ::ImxStorage* out) {
    out->element_type = ConvertNumericType(in.element_type());
    out->layout = ConvertLayout(in.layout());
}

// ConvertConversion
::imx::Conversion ConvertConversion(::ImxConversion in) {
    switch (in) {
        case IMX_CONVERT_NONE: return ::imx::NONE;
        case IMX_CONVERT_LOWBITS: return ::imx::LOWBITS;
    }
}

::ImxConversion ConvertConversion(::imx::Conversion in) {
    switch (in) {
        case ::imx::NONE: return IMX_CONVERT_NONE;
        case ::imx::LOWBITS: return IMX_CONVERT_LOWBITS;
    }
}

// ConvertBorderMode
::imx::BorderMode ConvertBorderMode(::ImxBorderMode in) {
    switch (in) {
        case IMX_BORDER_ZERO: return ::imx::ZERO;
        case IMX_BORDER_CONSTANT: return ::imx::CONSTANT;
        case IMX_BORDER_REPEAT_EDGE: return ::imx::REPEAT_EDGE;
        case IMX_BORDER_REPEAT_WIDE_EDGE: return ::imx::REPEAT_WIDE_EDGE;
        default: return ::imx::ZERO; // TODO
    }
}

::ImxBorderMode ConvertBorderMode(::imx::BorderMode in) {
    switch (in) {
        case ::imx::ZERO: return IMX_BORDER_ZERO;
        case ::imx::CONSTANT: return IMX_BORDER_CONSTANT;
        case ::imx::REPEAT_EDGE: return IMX_BORDER_REPEAT_EDGE;
        case ::imx::REPEAT_WIDE_EDGE: return IMX_BORDER_REPEAT_WIDE_EDGE;
    }
}

// ConvertBorder
void ConvertBorder(const ::ImxBorder& in, ::imx::Border* out) {
    out->set_mode(ConvertBorderMode(in.mode));
    out->set_edge_width(in.edge_width);
    // TODO other border values?
    out->set_border_value(in.border_value.int32);
}

void ConvertBorder(const ::imx::Border& in, ::ImxBorder* out) {
    out->mode = ConvertBorderMode(in.mode());
    out->edge_width = in.edge_width();
    // TODO other border values?
    out->border_value.int32 = in.border_value();
}

// ConvertMipiStreamIdentifier
void ConvertMipiStreamIdentifier(
    const ::ImxMipiStreamIdentifier& in, ::imx::MipiStreamIdentifier* out) {
    out->set_interface_id(in.interface_id);
    out->set_virtual_channel_id(in.virtual_channel_id);
    out->set_data_type(in.data_type);
}

void ConvertMipiStreamIdentifier(
    const ::imx::MipiStreamIdentifier& in, ::ImxMipiStreamIdentifier* out) {
    out->interface_id = in.interface_id();
    out->virtual_channel_id = in.virtual_channel_id();
    out->data_type = in.data_type();
}

// ConvertTransferNodeOverrides
void ConvertTransferNodeOverrides(
    const ::ImxTransferNodeOverrides& in,
    ::imx::TransferNodeOverrides* out) {
    out->set_skip_configure_linebuffer(in.skip_configure_linebuffer);
    out->set_skip_configure_dma(in.skip_configure_dma);
    out->set_override_linebuffer_num_consumers(
        in.override_linebuffer_num_consumers);
    out->set_linebuffer_num_consumers(in.linebuffer_num_consumers);
}

void ConvertTransferNodeOverrides(
    const ::imx::TransferNodeOverrides& in,
    ::ImxTransferNodeOverrides* out) {
    out->skip_configure_linebuffer = in.skip_configure_linebuffer();
    out->skip_configure_dma = in.skip_configure_dma();
    out->override_linebuffer_num_consumers =
        in.override_linebuffer_num_consumers();
    out->linebuffer_num_consumers = in.linebuffer_num_consumers();
}

// ConvertCreateTransferNodeInfo
void ConvertCreateTransferNodeInfo(
    const ::ImxCreateTransferNodeInfo& in,
    ::imx::CreateTransferNodeInfo* out) {
    out->set_use(ConvertParameterUse(in.use));
    ConvertParameterType(in.parameter_type, out->mutable_parameter_type());
    ConvertStorage(in.storage, out->mutable_storage());
    out->set_conversion(ConvertConversion(in.conversion));
    ConvertBorder(in.border, out->mutable_border());
    ConvertMipiStreamIdentifier(in.mipi_stream_id, out->mutable_mipi_stream_id());
    out->set_stripe_width(in.stripe_width);
    ConvertTransferNodeOverrides(
        in.transfer_node_overrides, out->mutable_transfer_node_overrides());
}
void ConvertCreateTransferNodeInfo(
    const ::imx::CreateTransferNodeInfo& in,
    ::ImxCreateTransferNodeInfo* out) {
    out->use = ConvertParameterUse(in.use());
    ConvertParameterType(in.parameter_type(), &out->parameter_type);
    ConvertStorage(in.storage(), &out->storage);
    out->conversion = ConvertConversion(in.conversion());
    ConvertBorder(in.border(), &out->border);
    ConvertMipiStreamIdentifier(in.mipi_stream_id(), &out->mipi_stream_id);
    out->stripe_width = in.stripe_width();
    ConvertTransferNodeOverrides(
        in.transfer_node_overrides(), &out->transfer_node_overrides);
}

// ConvertCompilerGraphOption
::imx::CompileGraphOption ConvertCompileGraphOption(::ImxCompileGraphOption in) {
    switch (in) {
        case IMX_OPTION_SIMULATOR_DUMP_PATH: return OPTION_SIMULATOR_DUMP_PATH;
        case IMX_OPTION_SIMULATOR_DUMP_IMAGE: return OPTION_SIMULATOR_DUMP_IMAGE;
        case IMX_OPTION_SIMULATOR_ENABLE_JIT: return OPTION_SIMULATOR_ENABLE_JIT;
        case IMX_OPTION_SIMULATOR_ENABLE_BINARY_PISA:
            return OPTION_SIMULATOR_ENABLE_BINARY_PISA;
        case IMX_OPTION_SIMULATOR_HW_CONFIG_FILE:
            return OPTION_SIMULATOR_HW_CONFIG_FILE;
        case IMX_OPTION_HISA:
            return OPTION_HISA;
        case IMX_OPTION_ENABLE_STRIPING:
            return OPTION_ENABLE_STRIPING;
        default: return OPTION_SIMULATOR_DUMP_PATH; // TODO()
    }
}

::ImxCompileGraphOption ConvertCompileGraphOption(::imx::CompileGraphOption in) {
    switch (in) {
        case OPTION_SIMULATOR_DUMP_PATH: return IMX_OPTION_SIMULATOR_DUMP_PATH;
        case OPTION_SIMULATOR_DUMP_IMAGE: return IMX_OPTION_SIMULATOR_DUMP_IMAGE;
        case OPTION_SIMULATOR_ENABLE_JIT: return IMX_OPTION_SIMULATOR_ENABLE_JIT;
        case OPTION_SIMULATOR_ENABLE_BINARY_PISA:
            return IMX_OPTION_SIMULATOR_ENABLE_BINARY_PISA;
        case OPTION_SIMULATOR_HW_CONFIG_FILE:
            return IMX_OPTION_SIMULATOR_HW_CONFIG_FILE;
        case OPTION_HISA:
            return IMX_OPTION_HISA;
        case OPTION_ENABLE_STRIPING:
            return IMX_OPTION_ENABLE_STRIPING;
    }
}

// ConvertOptionValueType
::imx::OptionValueType ConvertOptionValueType(::ImxOptionValueType in) {
    switch (in) {
        case IMX_OPTION_VALUE_TYPE_INT64: return OPTION_VALUE_TYPE_INT64;
        case IMX_OPTION_VALUE_TYPE_POINTER: return OPTION_VALUE_TYPE_POINTER;
        case IMX_OPTION_VALUE_TYPE_NONE: return OPTION_VALUE_TYPE_NONE;
    }
}

::ImxOptionValueType ConvertOptionValueType(::imx::OptionValueType in) {
    switch (in) {
        case OPTION_VALUE_TYPE_INT64: return IMX_OPTION_VALUE_TYPE_INT64;
        case OPTION_VALUE_TYPE_POINTER: return IMX_OPTION_VALUE_TYPE_POINTER;
        case OPTION_VALUE_TYPE_NONE: return IMX_OPTION_VALUE_TYPE_NONE;
    }
}

// ConvertBufferType
::imx::BufferType ConvertBufferType(::ImxBufferType in) {
    switch (in) {
        case IMX_DEVICE_BUFFER: return DEVICE_BUFFER;
        case IMX_MIPI_BUFFER: return MIPI_BUFFER;
        default: return DEVICE_BUFFER; // TODO(joshualitt) error
    }
}

::ImxBufferType ConvertBufferType(::imx::BufferType in) {
    switch (in) {
        case DEVICE_BUFFER: return IMX_DEVICE_BUFFER;
        case MIPI_BUFFER: return IMX_MIPI_BUFFER;
    }
}

// ConvertOptionValue
void ConvertOptionValue(const ::ImxOptionValue& in, ::imx::OptionValue* out) {
    out->set_type(ConvertOptionValueType(in.type));
    out->set_value(
        in.type == IMX_OPTION_VALUE_TYPE_INT64 ? in.i : (int64_t)in.p);
}

void ConvertOptionValue(const ::imx::OptionValue& in, ::ImxOptionValue* out) {
    out->type = ConvertOptionValueType(in.type());
    switch (in.type()) {
        case OPTION_VALUE_TYPE_INT64: out->i = in.value(); break;
        case OPTION_VALUE_TYPE_POINTER: out->p = (void*)in.value(); break;
        case OPTION_VALUE_TYPE_NONE: /*Do nothing*/ break;
    }
}

// ConvertCompileGraphOptionSetting
void ConvertCompileGraphOptionSetting(
    const ::ImxCompileGraphOptionSetting& in,
    ::imx::CompileGraphOptionSetting* out) {
    out->set_option(ConvertCompileGraphOption(in.option));
    ConvertOptionValue(in.value, out->mutable_value());
}

void ConvertCompileGraphOptionSetting(
    const ::imx::CompileGraphOptionSetting& in,
    ::ImxCompileGraphOptionSetting* out) {
    out->option = ConvertCompileGraphOption(in.option());
    ConvertOptionValue(in.value(), &out->value);
}

// ConvertParameterSetting
void ConvertParameterSetting(
    const ::ImxParameterSetting& in, ::imx::ParameterSetting* out) {
    out->set_node((int64_t)in.node);
    out->set_parameter_name(in.parameter_name);
    ConvertParameterType(in.type, out->mutable_type());
    // TODO(joshualitt) value?
}

void ConvertParameterSetting(
    const ::imx::ParameterSetting& in, ::ImxParameterSetting* out) {
    out->node = (ImxNodeHandle)in.node();
    // TODO(joshualitt) this is *crazy* dangerous.
    out->parameter_name = in.parameter_name().c_str();
    ConvertParameterType(in.type(), &out->type);
    // TODO(joshualitt) value?
}

// ConvertCompileGraphInfo
void ConvertCompileGraphInfo(
    const ::ImxCompileGraphInfo& in, ::imx::CompileGraphInfo* out) {
    out->set_device((int64_t)in.device);
    for (int i = 0; i < in.num_params; i++) {
        ConvertParameterSetting(in.params[i], out->add_params());
    }
    for (int i = 0; i < in.num_options; i++) {
        ConvertCompileGraphOptionSetting(in.options[i], out->add_options());
    }
}

// ConvertLateBufferConfig
void ConvertLateBufferConfig(
    const ::ImxLateBufferConfig& in, ::imx::LateBufferConfig* out) {
    out->set_buffer_type(ConvertBufferType(in.buffer_type));
    out->set_buffer_handle((int64_t)in.buffer);
    // TODO(joshualitt) planes other than 0?
    auto* plane = out->add_planes();
    plane->set_offset(in.plane[0].offset);
    for (int i = 0; i < IMX_DIM_MAX; i++) {
        plane->add_strides(in.plane[0].stride[i]);
    }
}

void ConvertLateBufferConfig(
    const ::imx::LateBufferConfig& in, ::ImxLateBufferConfig* out) {
    out->buffer_type = ConvertBufferType(in.buffer_type());
    out->buffer = (ImxDeviceBufferHandle)in.buffer_handle();
    // TODO(joshualitt) planes other than 0?
    out->plane[0].offset = in.planes(0).offset();
    for (int i = 0; i < IMX_DIM_MAX; i++) {
        out->plane[0].stride[i] = in.planes(0).strides(i);
    }
}

// ConvertFinalizeBufferInfo
void ConvertFinalizeBufferInfo(
    const ::ImxFinalizeBufferInfo& in, ::imx::FinalizeBufferInfo* out) {
    out->set_node_handle((int64_t)in.node);
    ConvertLateBufferConfig(in.config, out->mutable_config());
}

void ConvertFinalizeBufferInfo(
    const ::imx::FinalizeBufferInfo& in, ::ImxFinalizeBufferInfo* out) {
    out->node = (ImxNodeHandle)in.node_handle();
    ConvertLateBufferConfig(in.config(), &out->config);
}

}  // namespace imx
