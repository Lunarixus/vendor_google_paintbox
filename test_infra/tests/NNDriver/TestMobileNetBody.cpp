/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <cmath>

#include "NeuralNetworksOEM.h"
#include "NeuralNetworksWrapper.h"
#include "OemModel.h"

using namespace android::nn::wrapper;

namespace {

typedef float Matrix3x4[3][4];

class TestMobileNetBody : public ::testing::Test {};

#include "TestMobileNetBodyData.cpp"

void CreateMobileNetModel(Model *model) {
  OperandType oemModelType(Type::INT32, {});
  OperandType type0(Type::INT32, {1});
  OperandType type2(Type::TENSOR_FLOAT32, {1, 1, 1, 11});
  OperandType type32(Type::TENSOR_FLOAT32, {1, 1, 1, 256});
  OperandType type3(Type::TENSOR_FLOAT32, {1, 112, 112, 16});
  OperandType type6(Type::TENSOR_FLOAT32, {1, 14, 14, 128});
  OperandType type30(Type::TENSOR_FLOAT32, {1, 14, 14, 64});
  OperandType type1(Type::TENSOR_FLOAT32, {1, 224, 224, 3});
  OperandType type24(Type::TENSOR_FLOAT32, {1, 28, 28, 32});
  OperandType type25(Type::TENSOR_FLOAT32, {1, 28, 28, 64});
  OperandType type8(Type::TENSOR_FLOAT32, {1, 3, 3, 128});
  OperandType type16(Type::TENSOR_FLOAT32, {1, 3, 3, 16});
  OperandType type14(Type::TENSOR_FLOAT32, {1, 3, 3, 256});
  OperandType type22(Type::TENSOR_FLOAT32, {1, 3, 3, 32});
  OperandType type28(Type::TENSOR_FLOAT32, {1, 3, 3, 64});
  OperandType type18(Type::TENSOR_FLOAT32, {1, 56, 56, 16});
  OperandType type19(Type::TENSOR_FLOAT32, {1, 56, 56, 32});
  OperandType type10(Type::TENSOR_FLOAT32, {1, 7, 7, 128});
  OperandType type11(Type::TENSOR_FLOAT32, {1, 7, 7, 256});
  OperandType type34(Type::TENSOR_FLOAT32, {11, 1, 1, 256});
  OperandType type33(Type::TENSOR_FLOAT32, {11});
  OperandType type9(Type::TENSOR_FLOAT32, {128, 1, 1, 128});
  OperandType type31(Type::TENSOR_FLOAT32, {128, 1, 1, 64});
  OperandType type7(Type::TENSOR_FLOAT32, {128});
  OperandType type17(Type::TENSOR_FLOAT32, {16, 1, 1, 16});
  OperandType type5(Type::TENSOR_FLOAT32, {16, 3, 3, 3});
  OperandType type4(Type::TENSOR_FLOAT32, {16});
  OperandType type13(Type::TENSOR_FLOAT32, {256, 1, 1, 128});
  OperandType type15(Type::TENSOR_FLOAT32, {256, 1, 1, 256});
  OperandType type12(Type::TENSOR_FLOAT32, {256});
  OperandType type21(Type::TENSOR_FLOAT32, {32, 1, 1, 16});
  OperandType type23(Type::TENSOR_FLOAT32, {32, 1, 1, 32});
  OperandType type20(Type::TENSOR_FLOAT32, {32});
  OperandType type27(Type::TENSOR_FLOAT32, {64, 1, 1, 32});
  OperandType type29(Type::TENSOR_FLOAT32, {64, 1, 1, 64});
  OperandType type26(Type::TENSOR_FLOAT32, {64});

  auto select = model->addOperand(&oemModelType);
  auto b208 = model->addOperand(&type0);
  auto b209 = model->addOperand(&type0);
  auto b210 = model->addOperand(&type0);
  auto b211 = model->addOperand(&type0);
  auto b212 = model->addOperand(&type0);
  auto b213 = model->addOperand(&type0);
  auto b214 = model->addOperand(&type0);
  auto b215 = model->addOperand(&type0);
  auto b216 = model->addOperand(&type0);
  auto b217 = model->addOperand(&type0);
  auto op86 = model->addOperand(&type1);
  auto op85 = model->addOperand(&type2);
  auto op1 = model->addOperand(&type4);
  auto op2 = model->addOperand(&type5);
  auto op4 = model->addOperand(&type7);
  auto op5 = model->addOperand(&type8);
  auto op7 = model->addOperand(&type7);
  auto op8 = model->addOperand(&type9);
  auto op10 = model->addOperand(&type7);
  auto op11 = model->addOperand(&type8);
  auto op13 = model->addOperand(&type7);
  auto op14 = model->addOperand(&type9);
  auto op16 = model->addOperand(&type7);
  auto op17 = model->addOperand(&type8);
  auto op19 = model->addOperand(&type12);
  auto op20 = model->addOperand(&type13);
  auto op22 = model->addOperand(&type12);
  auto op23 = model->addOperand(&type14);
  auto op24 = model->addOperand(&type11);
  auto op25 = model->addOperand(&type12);
  auto op26 = model->addOperand(&type15);
  auto op28 = model->addOperand(&type4);
  auto op29 = model->addOperand(&type16);
  auto op31 = model->addOperand(&type4);
  auto op32 = model->addOperand(&type17);
  auto op34 = model->addOperand(&type4);
  auto op35 = model->addOperand(&type16);
  auto op37 = model->addOperand(&type20);
  auto op38 = model->addOperand(&type21);
  auto op40 = model->addOperand(&type20);
  auto op41 = model->addOperand(&type22);
  auto op43 = model->addOperand(&type20);
  auto op44 = model->addOperand(&type23);
  auto op46 = model->addOperand(&type20);
  auto op47 = model->addOperand(&type22);
  auto op49 = model->addOperand(&type26);
  auto op50 = model->addOperand(&type27);
  auto op52 = model->addOperand(&type26);
  auto op53 = model->addOperand(&type28);
  auto op55 = model->addOperand(&type26);
  auto op56 = model->addOperand(&type29);
  auto op58 = model->addOperand(&type26);
  auto op59 = model->addOperand(&type28);
  auto op61 = model->addOperand(&type7);
  auto op62 = model->addOperand(&type31);
  auto op64 = model->addOperand(&type7);
  auto op65 = model->addOperand(&type8);
  auto op67 = model->addOperand(&type7);
  auto op68 = model->addOperand(&type9);
  auto op70 = model->addOperand(&type7);
  auto op71 = model->addOperand(&type8);
  auto op73 = model->addOperand(&type7);
  auto op74 = model->addOperand(&type9);
  auto op76 = model->addOperand(&type7);
  auto op77 = model->addOperand(&type8);
  auto op79 = model->addOperand(&type7);
  auto op80 = model->addOperand(&type9);
  auto op81 = model->addOperand(&type32);
  auto op82 = model->addOperand(&type2);
  auto op83 = model->addOperand(&type33);
  auto op84 = model->addOperand(&type34);

  int32_t oemModel =
      static_cast<int32_t>(paintbox_nn::OemModel::MOBILE_NET_BODY);
  model->setOperandValue(select, &oemModel, sizeof(oemModel));

  int32_t b208_init[] = {2};
  model->setOperandValue(b208, b208_init, sizeof(int32_t) * 1);
  int32_t b209_init[] = {2};
  model->setOperandValue(b209, b209_init, sizeof(int32_t) * 1);
  int32_t b210_init[] = {2};
  model->setOperandValue(b210, b210_init, sizeof(int32_t) * 1);
  int32_t b211_init[] = {7};
  model->setOperandValue(b211, b211_init, sizeof(int32_t) * 1);
  int32_t b212_init[] = {7};
  model->setOperandValue(b212, b212_init, sizeof(int32_t) * 1);
  int32_t b213_init[] = {0};
  model->setOperandValue(b213, b213_init, sizeof(int32_t) * 1);
  int32_t b214_init[] = {1};
  model->setOperandValue(b214, b214_init, sizeof(int32_t) * 1);
  int32_t b215_init[] = {1};
  model->setOperandValue(b215, b215_init, sizeof(int32_t) * 1);
  int32_t b216_init[] = {1};
  model->setOperandValue(b216, b216_init, sizeof(int32_t) * 1);
  int32_t b217_init[] = {0};
  model->setOperandValue(b217, b217_init, sizeof(int32_t) * 1);

  model->setOperandValue(op1, (const void *)op1_init.data(),
                         sizeof(float) * 16);
  model->setOperandValue(op2, (const void *)op2_init.data(),
                         sizeof(float) * 432);
  model->setOperandValue(op4, (const void *)op4_init.data(),
                         sizeof(float) * 128);
  model->setOperandValue(op5, (const void *)op5_init.data(),
                         sizeof(float) * 1152);
  model->setOperandValue(op7, (const void *)op7_init.data(),
                         sizeof(float) * 128);
  model->setOperandValue(op8, (const void *)op8_init.data(),
                         sizeof(float) * 16384);
  model->setOperandValue(op10, (const void *)op10_init.data(),
                         sizeof(float) * 128);
  model->setOperandValue(op11, (const void *)op11_init.data(),
                         sizeof(float) * 1152);
  model->setOperandValue(op13, (const void *)op13_init.data(),
                         sizeof(float) * 128);
  model->setOperandValue(op14, (const void *)op14_init.data(),
                         sizeof(float) * 16384);
  model->setOperandValue(op16, (const void *)op16_init.data(),
                         sizeof(float) * 128);
  model->setOperandValue(op17, (const void *)op17_init.data(),
                         sizeof(float) * 1152);
  model->setOperandValue(op19, (const void *)op19_init.data(),
                         sizeof(float) * 256);
  model->setOperandValue(op20, (const void *)op20_init.data(),
                         sizeof(float) * 32768);
  model->setOperandValue(op22, (const void *)op22_init.data(),
                         sizeof(float) * 256);
  model->setOperandValue(op23, (const void *)op23_init.data(),
                         sizeof(float) * 2304);
  model->setOperandValue(op25, (const void *)op25_init.data(),
                         sizeof(float) * 256);
  model->setOperandValue(op26, (const void *)op26_init.data(),
                         sizeof(float) * 65536);
  model->setOperandValue(op28, (const void *)op28_init.data(),
                         sizeof(float) * 16);
  model->setOperandValue(op29, (const void *)op29_init.data(),
                         sizeof(float) * 144);
  model->setOperandValue(op31, (const void *)op31_init.data(),
                         sizeof(float) * 16);
  model->setOperandValue(op32, (const void *)op32_init.data(),
                         sizeof(float) * 256);
  model->setOperandValue(op34, (const void *)op34_init.data(),
                         sizeof(float) * 16);
  model->setOperandValue(op35, (const void *)op35_init.data(),
                         sizeof(float) * 144);
  model->setOperandValue(op37, (const void *)op37_init.data(),
                         sizeof(float) * 32);
  model->setOperandValue(op38, (const void *)op38_init.data(),
                         sizeof(float) * 512);
  model->setOperandValue(op40, (const void *)op40_init.data(),
                         sizeof(float) * 32);
  model->setOperandValue(op41, (const void *)op41_init.data(),
                         sizeof(float) * 288);
  model->setOperandValue(op43, (const void *)op43_init.data(),
                         sizeof(float) * 32);
  model->setOperandValue(op44, (const void *)op44_init.data(),
                         sizeof(float) * 1024);
  model->setOperandValue(op46, (const void *)op46_init.data(),
                         sizeof(float) * 32);
  model->setOperandValue(op47, (const void *)op47_init.data(),
                         sizeof(float) * 288);
  model->setOperandValue(op49, (const void *)op49_init.data(),
                         sizeof(float) * 64);
  model->setOperandValue(op50, (const void *)op50_init.data(),
                         sizeof(float) * 2048);
  model->setOperandValue(op52, (const void *)op52_init.data(),
                         sizeof(float) * 64);
  model->setOperandValue(op53, (const void *)op53_init.data(),
                         sizeof(float) * 576);
  model->setOperandValue(op55, (const void *)op55_init.data(),
                         sizeof(float) * 64);
  model->setOperandValue(op56, (const void *)op56_init.data(),
                         sizeof(float) * 4096);
  model->setOperandValue(op58, (const void *)op58_init.data(),
                         sizeof(float) * 64);
  model->setOperandValue(op59, (const void *)op59_init.data(),
                         sizeof(float) * 576);
  model->setOperandValue(op61, (const void *)op61_init.data(),
                         sizeof(float) * 128);
  model->setOperandValue(op62, (const void *)op62_init.data(),
                         sizeof(float) * 8192);
  model->setOperandValue(op64, (const void *)op64_init.data(),
                         sizeof(float) * 128);
  model->setOperandValue(op65, (const void *)op65_init.data(),
                         sizeof(float) * 1152);
  model->setOperandValue(op67, (const void *)op67_init.data(),
                         sizeof(float) * 128);
  model->setOperandValue(op68, (const void *)op68_init.data(),
                         sizeof(float) * 16384);
  model->setOperandValue(op70, (const void *)op70_init.data(),
                         sizeof(float) * 128);
  model->setOperandValue(op71, (const void *)op71_init.data(),
                         sizeof(float) * 1152);
  model->setOperandValue(op73, (const void *)op73_init.data(),
                         sizeof(float) * 128);
  model->setOperandValue(op74, (const void *)op74_init.data(),
                         sizeof(float) * 16384);
  model->setOperandValue(op76, (const void *)op76_init.data(),
                         sizeof(float) * 128);
  model->setOperandValue(op77, (const void *)op77_init.data(),
                         sizeof(float) * 1152);
  model->setOperandValue(op79, (const void *)op79_init.data(),
                         sizeof(float) * 128);
  model->setOperandValue(op80, (const void *)op80_init.data(),
                         sizeof(float) * 16384);
  model->setOperandValue(op83, (const void *)op83_init.data(),
                         sizeof(float) * 11);
  model->setOperandValue(op84, (const void *)op84_init.data(),
                         sizeof(float) * 2816);

  model->addOperation(ANEURALNETWORKS_OEM_OPERATION,
                      {select,         // oem model
                       op86,           // input
                       op2,    op1,    // conv
                       op29,   op28,   // depthwise_conv
                       op32,   op31,   // conv
                       op35,   op34,   // depthwise_conv
                       op38,   op37,   // conv
                       op41,   op40,   // depthwise_conv
                       op44,   op43,   // conv
                       op47,   op46,   // depthwise_conv
                       op50,   op49,   // conv
                       op53,   op52,   // depthwise_conv
                       op56,   op55,   // conv
                       op59,   op58,   // depthwise_conv
                       op62,   op61,   // conv
                       op65,   op64,   // depthwise_conv
                       op68,   op67,   // conv
                       op71,   op70,   // depthwise_conv
                       op74,   op73,   // conv
                       op77,   op76,   // depthwise_conv
                       op80,   op79,   // conv
                       op5,    op4,    // depthwise_conv
                       op8,    op7,    // conv
                       op11,   op10,   // depthwise_conv
                       op14,   op13,   // conv
                       op17,   op16,   // depthwise_conv
                       op20,   op19,   // conv
                       op23,   op22,   // depthwise_conv
                       op26,   op25},  // conv
                      {op24});         // output

  model->addOperation(ANEURALNETWORKS_AVERAGE_POOL_2D,
                      {op24, b208, b209, b210, b211, b212, b213}, {op81});
  model->addOperation(ANEURALNETWORKS_CONV_2D,
                      {op81, op84, op83, b214, b215, b216, b217}, {op82});
  model->addOperation(ANEURALNETWORKS_LOGISTIC, {op82}, {op85});

  model->identifyInputsAndOutputs({op86}, {op85});
  assert(model->isValid());
  model->finish();
}

int validateResults(const std::vector<float> &expected,
                    const std::vector<float> &actual) {
  int errors = 0;
  for (size_t i = 0; i < expected.size(); i++) {
    if (std::fabs(expected[i] - actual[i]) > 1.5e-5f) {
      std::cerr << " output[" << i << "] = " << actual[i] << " (should be "
                << expected[i] << ")\n";
      errors++;
    }
  }
  return errors;
}

TEST_F(TestMobileNetBody, MobileNet_Float_224) {
  Model model;
  CreateMobileNetModel(&model);

  Compilation compilation(&model);
  compilation.finish();
  Execution execution(&compilation);
  ASSERT_EQ(execution.setInput(0, static_cast<const void *>(inputData.data()),
                               inputData.size() * sizeof(float)),
            Result::NO_ERROR);
  std::vector<float> actualOutput(outputData.size());
  ASSERT_EQ(execution.setOutput(0, static_cast<void *>(actualOutput.data()),
                                actualOutput.size() * sizeof(float)),
            Result::NO_ERROR);
  ASSERT_EQ(execution.compute(), Result::NO_ERROR);
  ASSERT_EQ(validateResults(outputData, actualOutput), 0);
}

}  // namespace
