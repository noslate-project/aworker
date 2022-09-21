
// Ignore warnings: 'OSAtomicCompareAndSwap32' is deprecated: first deprecated
// in macOS 10.12

#ifndef SRC_IPC_IPC_PB_H_
#define SRC_IPC_IPC_PB_H_
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "proto/alice.pb.h"
#include "proto/rpc.pb.h"
#pragma GCC diagnostic pop

#endif  // SRC_IPC_IPC_PB_H_
