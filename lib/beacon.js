'use strict';

const agentChannel = load('agent_channel');
const { bufferLikeToUint8Array } = load('utils');
const debug = load('console/debuglog').debuglog('beacon');
const { TextDecoder } = load('encoding');

const kDecoder = new TextDecoder();

async function request(operation, metadata, value) {
  const { status, body } = await agentChannel.call('extensionBinding', {
    name: 'beacon',
    operation,
    metadata: JSON.stringify(metadata),
    body: value,
  });
  if (status !== 200) {
    const message = kDecoder.decode(body);
    throw new Error(`Failed to send beacon: status(${status}) ${message}`);
  }
}

function sendBeacon(type, options, data) {
  if (typeof type !== 'string') {
    throw TypeError('type must be a string');
  }
  const format = options.format;
  if (format != null && typeof format !== 'string') {
    throw TypeError('options.format must be a string');
  }
  request('send', {
    type,
    format,
  }, bufferLikeToUint8Array(data))
    .catch(e => {
      // TODO(chengzhong.wcz): aworker logger
      debug(e);
    });
  return true;
}

wrapper.mod = {
  sendBeacon,
};
