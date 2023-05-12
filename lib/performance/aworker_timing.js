'use strict';
const { loopIdleTime } = loadBinding('perf');
const { milestones } = loadBinding('constants');
const { getMilestoneRelativeTimestamp, now } = load('performance/utils');

const {
  AWORKER_PERFORMANCE_MILESTONE_BOOTSTRAP_AGENT,
  AWORKER_PERFORMANCE_MILESTONE_READ_SCRIPT_FILE,
  AWORKER_PERFORMANCE_MILESTONE_PARSE_SCRIPT,
  AWORKER_PERFORMANCE_MILESTONE_EVALUATE_SCRIPT,
  AWORKER_PERFORMANCE_MILESTONE_AFTER_FORK,
  AWORKER_PERFORMANCE_MILESTONE_LOOP_START,
  AWORKER_PERFORMANCE_MILESTONE_EVALUATE,
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
      bootStrapTime: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_BOOTSTRAP_AGENT);
        },
      },
      readScriptFile: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_READ_SCRIPT_FILE);
        },
      },
      parseScript: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_PARSE_SCRIPT);
        },
      },
      evaluateScript: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_EVALUATE_SCRIPT);
        },
      },
      loopStart: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_LOOP_START);
        },
      },
      evaluate: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_EVALUATE);
        },
      },
      afterFork: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_AFTER_FORK);
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
