'use strict';

const { getEventInternalData, Event } = load('dom/event');

class ErrorEvent extends Event {
  constructor(type, init) {
    super(type, init);

    Object.defineProperties(this, {
      message: {
        enumerable: true,
        get() {
          return getEventInternalData(this).error.message;
        },
      },

      filename: {
        enumerable: true,
        get() {
          // TODO(kaidi.zkd): return the real value
          return 'dummy-filename';
        },
      },

      lineno: {
        enumerable: true,
        get() {
          // TODO(kaidi.zkd): return the real value
          return 0;
        },
      },

      colno: {
        enumerable: true,
        get() {
          // TODO(kaidi.zkd): return the real value
          return 0;
        },
      },

      error: {
        enumerable: true,
        get() {
          return getEventInternalData(this).error;
        },
      },
    });
  }
}

function createErrorEvent(type, e) {
  const event = new ErrorEvent(type, {
    cancelable: true,
  });

  const data = getEventInternalData(event);
  data.error = e;

  return event;
}

class PromiseRejectionEvent extends Event {
  constructor(type, init) {
    super(type, init);

    Object.defineProperties(this, {
      promise: {
        enumerable: true,
        get() {
          return getEventInternalData(this).promise;
        },
      },

      reason: {
        enumerable: true,
        get() {
          return getEventInternalData(this).reason;
        },
      },
    });
  }
}

function createPromiseRejectionEvent(type, promise, reason) {
  const event = new PromiseRejectionEvent(type, {
    cancelable: true,
  });

  const data = getEventInternalData(event);
  data.promise = promise;
  data.reason = reason;

  return event;
}

class ForkedProcessItemExitEvent extends Event {
  constructor() {
    super('exit');
    Object.defineProperty(this, 'code', {
      enumerable: true,
      get() {
        return getEventInternalData(this).code;
      },
    });
  }
}

function createForkedProcessItemExitEvent(code) {
  const event = new ForkedProcessItemExitEvent();
  const data = getEventInternalData(event);
  data.code = code;
  return event;
}

wrapper.mod = {
  ErrorEvent,
  createErrorEvent,
  PromiseRejectionEvent,
  createPromiseRejectionEvent,

  ForkedProcessItemExitEvent,
  createForkedProcessItemExitEvent,
};
