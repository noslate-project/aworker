'use strict';

const { customInspectSymbol } = load('console/console');
const {
  parseSearchParams,
  serializeSearchParams,

  parseUrl,
  // TODO: strict WHATWG URL state override parse
  updateProtocol,
  updateUserInfo,
  updateHost,
  updateHostname,
  updatePort,
  updatePathname,
  updateSearch,
  updateHash,
} = loadBinding('url');
const {
  toUSVString,
  isIterable,
  defineIDLClassMembers,
  requireIdlInternalSlot,
} = load('types');

const kURLUrl = Symbol('URL#url');
const kURLUrlSearchParams = Symbol('URL#urlsearchparams');
const kURLSearchParamsUrl = Symbol('URLSearchParams#url');
const kURLSearchParamsList = Symbol('URLSearchParams#urlSearchParams');

// 避免循环引用
let requiredArguments = function prepareRequiredArguments(...args) {
  requiredArguments = load('utils').requiredArguments;
  return requiredArguments(...args);
};

class URLSearchParams {
  [kURLSearchParamsUrl];
  [kURLSearchParamsList];

  constructor(init = '') {
    if (typeof init !== 'string' && isIterable(init)) {
      init = [ ...init ];
    }

    if (Array.isArray(init)) {
      for (let i = 0; i < init.length; i++) {
        const tuple = init[i];
        if (!tuple || tuple.length !== 2) {
          throw new TypeError('URLSearchParams.constructor tuple array argument must only contain pair elements');
        }

        if (!Array.isArray(tuple)) {
          init[i] = [ toUSVString(tuple[0]), toUSVString(tuple[1]) ];
        } else {
          tuple[0] = toUSVString(tuple[0]);
          tuple[1] = toUSVString(tuple[1]);
        }
      }
    } else if ((typeof init === 'object' || typeof init === 'function') && !(init instanceof URLSearchParams)) {
      if (init instanceof Error) {
        // TODO(kaidi.zkd): what does new URLSearchParams(DOMException.prototype) really should do?
        throw new TypeError('Invalid parameters');
      }
      const array = [];
      const visited = {};
      for (const key of Object.keys(init)) {
        const replaced = toUSVString(key);
        if (visited[replaced]) {
          visited[replaced][1] = toUSVString(init[key]);
        } else {
          const temp = [ replaced, toUSVString(init[key]) ];
          visited[replaced] = temp;
          array.push(temp);
        }
      }
      init = array;
    }

    if (typeof init === 'string') {
      this[kURLSearchParamsList] = parseSearchParams(init);
    } else if (Array.isArray(init)) {
      this[kURLSearchParamsList] = init;
    } else {
      this[kURLSearchParamsList] = [];
    }
  }

  forEach(callback) {
    requiredArguments('URLSearchParams.forEach', arguments.length, 1);

    if (arguments.length >= 2 && typeof arguments[1] !== 'undefined') {
      callback = callback.bind(arguments[1]);
    }

    for (const entry of this.entries()) {
      callback(entry[1], entry[0], this);
    }
  }

  * keys() {
    for (const entry of this.entries()) {
      yield entry[0];
    }
  }

  * values() {
    for (const entry of this.entries()) {
      yield entry[1];
    }
  }

  * entries() {
    for (let idx = 0; idx < this[kURLSearchParamsList].length; idx++) {
      const entry = this[kURLSearchParamsList][idx];
      yield [ entry[0], entry[1] ];
    }
  }

  append(name, value) {
    requiredArguments('URLSearchParams.append', arguments.length, 2);
    this[kURLSearchParamsList].push([ String(name), String(value) ]);

    updateURLSearch(this[kURLSearchParamsUrl], this.toString());
  }

  delete(name) {
    requiredArguments('URLSearchParams.delete', arguments.length, 1);
    name = String(name);
    let updated = false;
    for (let idx = 0; idx < this[kURLSearchParamsList].length;) {
      if (this[kURLSearchParamsList][idx][0] === name) {
        this[kURLSearchParamsList].splice(idx, 1);
        updated = true;
      } else {
        idx++;
      }
    }

    if (updated) {
      updateURLSearch(this[kURLSearchParamsUrl], this.toString());
    }
  }

  getAll(name) {
    requiredArguments('URLSearchParams.getAll', arguments.length, 1);
    name = String(name);
    const result = [];
    for (let idx = 0; idx < this[kURLSearchParamsList].length; idx++) {
      const entry = this[kURLSearchParamsList][idx];
      if (entry[0] === name) {
        result.push(entry[1]);
      }
    }

    return result;
  }

  get(name) {
    requiredArguments('URLSearchParams.get', arguments.length, 1);
    name = String(name);
    for (let idx = 0; idx < this[kURLSearchParamsList].length; idx++) {
      const entry = this[kURLSearchParamsList][idx];
      if (entry[0] === name) {
        return entry[1];
      }
    }
    return null;
  }

  has(name) {
    requiredArguments('URLSearchParams.has', arguments.length, 1);
    name = String(name);
    for (let idx = 0; idx < this[kURLSearchParamsList].length; idx++) {
      const entry = this[kURLSearchParamsList][idx];
      if (entry[0] === name) {
        return true;
      }
    }
    return false;
  }

  set(name, value) {
    requiredArguments('URLSearchParams.set', arguments.length, 2);
    name = String(name);
    value = String(value);
    let found = false;
    for (let idx = 0; idx < this[kURLSearchParamsList].length;) {
      const entry = this[kURLSearchParamsList][idx];
      if (entry[0] !== name) {
        idx++;
        continue;
      }
      if (!found) {
        entry[1] = value;
        found = true;
        idx++;
      } else {
        this[kURLSearchParamsList].splice(idx, 1);
      }
    }

    if (!found) {
      this[kURLSearchParamsList].push([ name, value ]);
    }

    updateURLSearch(this[kURLSearchParamsUrl], this.toString());
  }

  sort() {
    this[kURLSearchParamsList].sort((lhs, rhs) => {
      const a = lhs[0];
      const b = rhs[0];
      return (a === b ? 0 : (a > b ? 1 : -1));
    });

    updateURLSearch(this[kURLSearchParamsUrl], this.toString());
  }

  toString() {
    requireIdlInternalSlot(this, kURLSearchParamsList);
    return serializeSearchParams(this[kURLSearchParamsList]);
  }

  [customInspectSymbol]() {
    const obj = {};

    Object.defineProperty(obj, 'constructor', {
      enumerable: false,
      value: URLSearchParams,
    });

    for (const [ key, value ] of this.entries()) {
      obj[key] = value;
    }

    return obj;
  }
}

defineIDLClassMembers(URLSearchParams, 'URLSearchParams', [
  'append',
  'delete',
  'getAll',
  'get',
  'has',
  'set',
  'sort',
  'toString',
], {
  iterable: true,
});

class URL {
  [kURLUrl];
  [kURLUrlSearchParams];

  constructor(url, ...args) {
    url = String(url);

    let base;
    if (args.length >= 1) {
      base = args[0];
    }

    if (base != null) {
      this[kURLUrl] = parseUrl(url, String(base));
    } else {
      this[kURLUrl] = parseUrl(url);
    }
    this[kURLUrlSearchParams] = createURLUrlSearchParams(this, this[kURLUrl].search);
  }

  get hash() {
    return this[kURLUrl].hash;
  }

  set hash(hash) {
    this[kURLUrl].hash = updateHash(String(hash));
    this[kURLUrl].href = toHref(this[kURLUrl]);
  }

  get host() {
    if (this[kURLUrl].port !== '') {
      return `${this[kURLUrl].hostname}:${this[kURLUrl].port}`;
    }
    return `${this[kURLUrl].hostname}`;
  }

  set host(host) {
    const [ hostname, port ] = updateHost(this[kURLUrl].protocol, String(host));
    if (hostname === '') {
      return;
    }
    this[kURLUrl].hostname = hostname;
    if (port !== '') {
      this[kURLUrl].port = port;
    }
    this[kURLUrl].href = toHref(this[kURLUrl]);
  }

  get hostname() {
    return this[kURLUrl].hostname;
  }

  set hostname(hostname) {
    this[kURLUrl].hostname = updateHostname(this[kURLUrl].protocol, String(hostname));
    this[kURLUrl].href = toHref(this[kURLUrl]);
  }

  get href() {
    return this[kURLUrl].href;
  }

  set href(href) {
    this[kURLUrl] = parseUrl(String(href));
    updateURLUrlSearchParams(this[kURLUrlSearchParams], this[kURLUrl].search);
  }

  get password() {
    return this[kURLUrl].password;
  }

  set password(password) {
    this[kURLUrl].password = updateUserInfo(String(password));
    this[kURLUrl].href = toHref(this[kURLUrl]);
  }

  get pathname() {
    return this[kURLUrl].pathname;
  }

  set pathname(pathname) {
    this[kURLUrl].pathname = updatePathname(this[kURLUrl].protocol, String(pathname));
    this[kURLUrl].href = toHref(this[kURLUrl]);
  }

  get port() {
    return this[kURLUrl].port;
  }

  set port(port) {
    this[kURLUrl].port = updatePort(this[kURLUrl].protocol, String(port));
    this[kURLUrl].href = toHref(this[kURLUrl]);
  }

  get protocol() {
    return this[kURLUrl].protocol;
  }

  set protocol(protocol) {
    this[kURLUrl].protocol = updateProtocol(String(protocol));
    this[kURLUrl].href = toHref(this[kURLUrl]);
  }

  get search() {
    return this[kURLUrl].search;
  }

  set search(search) {
    this[kURLUrl].search = updateSearch(this[kURLUrl].protocol, String(search));
    this[kURLUrl].href = toHref(this[kURLUrl]);
    updateURLUrlSearchParams(this[kURLUrlSearchParams], this[kURLUrl].search);
  }

  get username() {
    return this[kURLUrl].username;
  }

  set username(username) {
    this[kURLUrl].username = updateUserInfo(String(username));
    this[kURLUrl].href = toHref(this[kURLUrl]);
  }

  get searchParams() {
    requireIdlInternalSlot(this, kURLUrlSearchParams);
    return this[kURLUrlSearchParams];
  }

  get origin() {
    return this[kURLUrl].origin;
  }

  toString() {
    if (!(this instanceof URL)) {
      throw new TypeError('Invalid URL object.');
    }

    return this.href;
  }

  toJSON() {
    if (!(this instanceof URL)) {
      throw new TypeError('Invalid URL object.');
    }

    return this.href;
  }

  [customInspectSymbol]() {
    const obj = {
      href: this.href,
      origin: this.origin,
      protocol: this.protocol,
      username: this.username,
      password: this.password,
      host: this.host,
      hostname: this.hostname,
      port: this.port,
      pathname: this.pathname,
      hash: this.hash,
      search: this.search,
      searchParams: this.searchParams,
    };

    Object.defineProperty(obj, 'constructor', {
      enumerable: false,
      value: URL,
    });

    return obj;
  }
}

defineIDLClassMembers(URL, 'URL', [
  'hash',
  'host',
  'hostname',
  'href',
  'password',
  'pathname',
  'port',
  'protocol',
  'search',
  'username',
  'username',
  'searchParams',
  'origin',
  'toString',
  'toJSON',
]);

function updateURLSearch(url, search) {
  if (url == null) {
    return;
  }
  url[kURLUrl].search = updateSearch(url[kURLUrl].protocol, String(search));
  url[kURLUrl].href = toHref(url[kURLUrl]);
}

function updateURLUrlSearchParams(searchParams, search) {
  searchParams[kURLSearchParamsList] = parseSearchParams(search);
}

function createURLUrlSearchParams(url, search) {
  const searchParams = new URLSearchParams(search);
  searchParams[kURLSearchParamsUrl] = url;
  return searchParams;
}

// TODO: strict WHATWG URL state override parse
function toHref(urlState) {
  const authentication = (urlState.username || urlState.password) ? `${urlState.username}:${urlState.password}@` : '';
  const port = urlState.port;
  const host = port ? `${urlState.hostname}:${port}` : urlState.hostname;
  const slashes = host ? '//' : '';
  let pathname = urlState.pathname;
  if (pathname && pathname.charCodeAt(0) !== 0x2f /* '/' */ && host.length) {
    pathname = `/${pathname}`;
  }
  return `${urlState.protocol}${slashes}${authentication}${host}${pathname}${urlState.search}${urlState.hash}`;
}

wrapper.mod = {
  URL,
  URLSearchParams,
};
