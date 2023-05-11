'use strict';

const { hrtime, getTimeOrigin, getTimeOriginTimeStamp, loopIdleTime, getMilestonesTimestamp } = loadBinding('perf');

function now() {
  return hrtime() - getTimeOrigin();
}

function eventLoopUtilization(util1, util2) {
  // Get the original milestone timestamps that calculated from the beginning
  // of the process.
  return internalEventLoopUtilization(
    getTimeOrigin(),
    loopIdleTime(),
    util1,
    util2
  );
}

function internalEventLoopUtilization(timeOrigin, loopIdleTime, util1, util2) {
  if (timeOrigin <= 0) {
    return { idle: 0, active: 0, utilization: 0 };
  }

  if (util2) {
    const idle = util1.idle - util2.idle;
    const active = util1.active - util2.active;
    return { idle, active, utilization: active / (idle + active) };
  }

  // Using process.hrtime() to get the time from the beginning of the process,
  // and offset it by the loopStart time (which is also calculated from the
  // beginning of the process).
  const active = hrtime() - timeOrigin - loopIdleTime;

  if (!util1) {
    return {
      idle: loopIdleTime,
      active,
      utilization: active / (loopIdleTime + active),
    };
  }

  const idleDelta = loopIdleTime - util1.idle;
  const activeDelta = active - util1.active;
  const utilization = activeDelta / (idleDelta + activeDelta);
  return {
    idle: idleDelta,
    active: activeDelta,
    utilization,
  };
}

function getMilestoneTimestamp(milestoneIdx) {
  const milestonesTimeStamp = getMilestonesTimestamp();
  const ts = milestonesTimeStamp[milestoneIdx];
  if (ts === -1) {
    return ts;
  }
  return ts / 1e3;
}

function getMilestoneRelativeTimestamp(milestoneIdx) {
  const ts = getMilestoneTimestamp(milestoneIdx);
  if (ts === 0) {
    return 0;
  }
  return ts - getTimeOriginTimeStamp();
}

wrapper.mod = {
  now,
  eventLoopUtilization,
  getMilestoneTimestamp,
  getMilestoneRelativeTimestamp,
};
