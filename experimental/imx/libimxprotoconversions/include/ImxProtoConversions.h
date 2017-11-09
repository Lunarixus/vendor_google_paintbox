#ifndef PAINTBOX_IMX_PROTO_CONVERSIONS_H
#define PAINTBOX_IMX_PROTO_CONVERSIONS_H

#include "Imx.h"
#include "vendor/google_paintbox/experimental/imx/libimxproto/include/imx.pb.h"

namespace imx {

::imx::ImxError ConvertStatus(::ImxError result);
::ImxError ConvertStatus(::imx::ImxError result);

::imx::ParameterUse ConvertParameterUse(::ImxParameterUse in);
::ImxParameterUse ConvertParameterUse(::imx::ParameterUse in);

::imx::NumericType ConvertNumericType(::ImxNumericType in);
::ImxNumericType ConvertNumericType(::imx::NumericType in);

::imx::SizeKind ConvertSizeKind(::ImxSizeKind in);
::ImxSizeKind ConvertSizeKind(::imx::SizeKind in);

void ConvertShape(const ::ImxShape& in, ::imx::Shape* out);
void ConvertShape(const ::imx::Shape& in, ::ImxShape* out);

void ConvertParameterType(
    const ::ImxParameterType& in, ::imx::ParameterType* out);
void ConvertParameterType(
    const ::imx::ParameterType& in, ::ImxParameterType* out);

::imx::Layout ConvertLayout(::ImxLayout in);
::ImxLayout ConvertLayout(::imx::Layout in);

void ConvertStorage(const ::ImxStorage& in, ::imx::Storage* out);
void ConvertStorage(const ::imx::Storage& in, ::ImxStorage* out);

::imx::Conversion ConvertConversion(::ImxConversion in);
::ImxConversion ConvertConversion(::imx::Conversion in);

::imx::BorderMode ConvertBorderMode(::ImxBorderMode in);
::ImxBorderMode ConvertBorderMode(::imx::BorderMode in);

void ConvertBorder(const ::ImxBorder& in, ::imx::Border* out);
void ConvertBorder(const ::imx::Border& in, ::ImxBorder* out);

void ConvertMipiStreamIdentifier(
    const ::ImxMipiStreamIdentifier& in, ::imx::MipiStreamIdentifier* out);
void ConvertMipiStreamIdentifier(
    const ::imx::MipiStreamIdentifier& in, ::ImxMipiStreamIdentifier* out);

void ConvertTransferNodeOverrides(
    const ::ImxTransferNodeOverrides& in,
    ::imx::TransferNodeOverrides* out);
void ConvertTransferNodeOverrides(
    const ::imx::TransferNodeOverrides& in,
    ::ImxTransferNodeOverrides* out);

void ConvertCreateTransferNodeInfo(
    const ::ImxCreateTransferNodeInfo& in,
    ::imx::CreateTransferNodeInfo* out);
void ConvertCreateTransferNodeInfo(
    const ::imx::CreateTransferNodeInfo& in,
    ::ImxCreateTransferNodeInfo* out);

::imx::CompileGraphOption ConvertCompileGraphOption(::ImxCompileGraphOption in);
::ImxCompileGraphOption ConvertCompileGraphOption(::imx::CompileGraphOption in);

::imx::OptionValueType ConvertOptionValueType(::ImxOptionValueType in);
::ImxOptionValueType ConvertOptionValueType(::imx::OptionValueType in);

::imx::BufferType ConvertBufferType(::ImxBufferType in);
::ImxBufferType ConvertBufferType(::imx::BufferType in);

void ConvertOptionValue(const ::ImxOptionValue& in, ::imx::OptionValue* out);
void ConvertOptionValue(const ::imx::OptionValue& in, ::ImxOptionValue* out);

void ConvertCompileGraphOptionSetting(
    const ::ImxCompileGraphOptionSetting& in,
    ::imx::CompileGraphOptionSetting* out);
void ConvertCompileGraphOptionSetting(
    const ::imx::CompileGraphOptionSetting& in,
    ::ImxCompileGraphOptionSetting* out);

void ConvertParameterSetting(
    const ::ImxParameterSetting& in, ::imx::ParameterSetting* out);
void ConvertParameterSetting(
    const ::imx::ParameterSetting& in, ::ImxParameterSetting* out);

// One way conversion for CompileGraph
void ConvertCompileGraphInfo(
    const ::ImxCompileGraphInfo& in, ::imx::CompileGraphInfo* out);

void ConvertLateBufferConfig(
    const ::ImxLateBufferConfig& in, ::imx::LateBufferConfig* out);
void ConvertLateBufferConfig(
    const ::imx::LateBufferConfig& in, ::ImxLateBufferConfig* out);

void ConvertFinalizeBufferInfo(
    const ::ImxFinalizeBufferInfo& in, ::imx::FinalizeBufferInfo* out);
void ConvertFinalizeBufferInfo(
    const ::imx::FinalizeBufferInfo& in, ::ImxFinalizeBufferInfo* out);

}  // namespace imx
#endif // PAINTBOX_IMX_PROTO_CONVERSIONS
