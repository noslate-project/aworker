// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <cstddef>
#include <cstdint>

#include "modp_b64.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Create a buffer to store the output.
  std::string buffer;
  buffer.resize(modp_b64_decode_len(size));
  modp_b64_decode(&(buffer[0]), (const char *) data, size);
  return 0;
}
