#ifndef PAINTBOX_NN_RPC_H
#define PAINTBOX_NN_RPC_H

// RPC channel ID
enum channel {
  PREPARE_MODEL = 1,
  EXECUTE = 2,
  DESTROY_MODEL = 3,
};

#endif  // PAINTBOX_NN_RPC_H