'use strict';

const { userAgent } = load('navigator');

const allowedProtocols = [
  'http:',
  'https:',
];

const userAgentHeaders = [
  [ 'accept', '*/*' ],
  [ 'accept-language', '*' ],
  // TODO: User-Agent header content
  [ 'user-agent', userAgent ],
];

const kCRLF = '\r\n';

wrapper.mod = {
  allowedProtocols,
  userAgentHeaders,
  kCRLF,
};
