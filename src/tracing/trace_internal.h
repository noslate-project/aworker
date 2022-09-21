#pragma once

#define TRACING_CATEGORY_AWORKER "aworker"
#define TRACING_CATEGORY_AWORKER1(one)                                         \
  TRACING_CATEGORY_AWORKER "," TRACING_CATEGORY_AWORKER "." #one
#define TRACING_CATEGORY_AWORKER2(one, two)                                    \
  TRACING_CATEGORY_AWORKER "," TRACING_CATEGORY_AWORKER "." #one               \
                           "," TRACING_CATEGORY_AWORKER "." #one "." #two
