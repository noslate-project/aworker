'use strict';
const { loopIdleTime } = loadBinding('perf');
const { milestones } = loadBinding('constants');
const { getMilestoneRelativeTimestamp, getMilestoneTimestamp, now } = load('performance/utils');

const {
  AWORKER_PERFORMANCE_MILESTONE_INITIALIZE_PER_PROCESS,
  AWORKER_PERFORMANCE_MILESTONE_BOOTSTRAP_AGENT,
  AWORKER_PERFORMANCE_MILESTONE_EXECUTE_MAIN,
  AWORKER_PERFORMANCE_MILESTONE_CACHE_SOURCE_MAP,
  AWORKER_PERFORMANCE_MILESTONE_READ_SCRIPT_FILE,
  AWORKER_PERFORMANCE_MILESTONE_PARSE_SCRIPT,
  AWORKER_PERFORMANCE_MILESTONE_EVALUATE_SCRIPT,
  AWORKER_PERFORMANCE_MILESTONE_AFTER_FORK,
  AWORKER_PERFORMANCE_MILESTONE_LOOP_START,
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
      loopStart: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneTimestamp(AWORKER_PERFORMANCE_MILESTONE_LOOP_START);
        },
      },
      initializeProcess: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_INITIALIZE_PER_PROCESS);
        },
      },
      bootStrapTime: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_BOOTSTRAP_AGENT);
        },
      },
      executeMain: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_EXECUTE_MAIN);
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
      cacheSourceMap: {
        __proto__: null,
        enumerable: true,
        configurable: true,
        get() {
          return getMilestoneRelativeTimestamp(AWORKER_PERFORMANCE_MILESTONE_CACHE_SOURCE_MAP);
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
