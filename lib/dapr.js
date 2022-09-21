'use strict';

const DaprV1$0 = load('dapr/1.0');

const Dapr = {};
let daprV1$0;

Object.defineProperties(Dapr, {
  '1.0': {
    enumerable: false,
    configurable: true,
    get: () => {
      if (daprV1$0 === undefined) {
        daprV1$0 = new DaprV1$0();
      }
      return daprV1$0;
    },
  },
});

wrapper.mod = {
  Dapr,
};
