'use strict';
const { loopIdleTime } = loadBinding('perf');
const { milestones } = loadBinding('constants');
const { getMilestoneRelativeTimestamp, now } = load('performance/utils');

const {
  AWORKER_PERFORMANCE_MILESTONE_BOOTSTRAP_AGENT,
  AWORKER_PERFORMANCE_MILESTONE_LOAD_EVENT,
} = milestones;

class PerformanceAworkerTiming {
  constructor() {
    Object.defineProperties(this, {
      name: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        value: 'aworker',
      },
      entryType: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        value: 'aworker',
      },
      startTime: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return 0;
        },
      },
      duration: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get: now,
      },
      bootstrapAgent: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_BOOTSTRAP_AGENT);
        },
      },
      loadEvent: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_LOAD_EVENT);
        },
      },
      idleTime: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get: loopIdleTime,
      },
    });
  }
}

wrapper.mod = new PerformanceAworkerTiming();
