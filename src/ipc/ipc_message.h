#ifndef SRC_IPC_IPC_MESSAGE_H_
#define SRC_IPC_IPC_MESSAGE_H_
#include "ipc/ipc_pb.h"

// TODO(chengzhong.wcz): connect timeout by args?
#define NOSLATED_CONNECT_TIMEOUT_MS 5000
#define NOSLATED_STREAM_PUSH_TIMEOUT_MS 30000
#define NOSLATED_RESOURCE_NOTIFICATION_TIMEOUT_MS 2000
#define NOSLATED_COLLECT_METRICS_TIMEOUT_MS 2000
#define NOSLATED_INSPECTOR_TIMEOUT_MS 60000

#define NOSLATED_REQUEST_TYPES(V)                                                 \
  V(Credentials)                                                               \
  V(StreamOpen)                                                                \
  V(StreamPush)                                                                \
  V(CollectMetrics)                                                            \
  V(Fetch)                                                                     \
  V(FetchAbort)                                                                \
  V(DaprInvoke)                                                                \
  V(DaprBinding)                                                               \
  V(ExtensionBinding)                                                          \
  V(ResourceNotification)                                                      \
  V(ResourcePut)                                                               \
  V(Trigger)                                                                   \
  V(InspectorStart)                                                            \
  V(InspectorStartSession)                                                     \
  V(InspectorEndSession)                                                       \
  V(InspectorGetTargets)                                                       \
  V(InspectorCommand)                                                          \
  V(InspectorEvent)                                                            \
  V(InspectorStarted)                                                          \
  V(TracingStart)                                                              \
  V(TracingStop)

#define NOSLATED_CANONICAL_CODE_KEYS(V)                                           \
  V(OK)                                                                        \
  V(INTERNAL_ERROR)                                                            \
  V(TIMEOUT)                                                                   \
  V(NOT_IMPLEMENTED)                                                           \
  V(CONNECTION_RESET)                                                          \
  V(CLIENT_ERROR)                                                              \
  V(CANCELLED)

#define NOSLATED_CREDENTIAL_TARGET_TYPE_KEYS(V)                                   \
  V(Data)                                                                      \
  V(Diagnostics)

#define NOSLATED_RESOURCE_PUT_ACTION_KEYS(V)                                      \
  V(ACQUIRE_SH)                                                                \
  V(ACQUIRE_EX)                                                                \
  V(RELEASE)

#endif  // SRC_IPC_IPC_MESSAGE_H_
