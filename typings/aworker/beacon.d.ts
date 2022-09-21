namespace aworker {
  interface BeaconOptions {
    format?: string;
  }

  function sendBeacon(type: string, options: BeaconOptions, data: BufferLike): boolean;
}
