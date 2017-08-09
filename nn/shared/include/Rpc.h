#ifndef PAINTBOX_NN_RPC_H
#define PAINTBOX_NN_RPC_H

// In current NN API runtime,
// There are only two pools, one for input and the other for output.
// Once custom buffer pools are supported.
// This assunption needs to be changed.
#define INPUT_POOL 0
#define OUTPUT_POOL 1

// RPC channel ID
enum channel {
  PREPARE_MODEL = 1,
  EXECUTE = 2,
};

#endif  // PAINTBOX_NN_RPC_H