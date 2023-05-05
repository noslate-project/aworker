'use strict';

const { Body, Response } = load('fetch/body');

function objectToKeyValueMap(object) {
  const entries = Object.entries(object)
    .map(([ key, value ]) => [ key, `${value}` ]);
  return Object.fromEntries(entries);
}

class ServiceRequest extends Body {
  #app;
  #method;
  constructor(init) {
    init = init ?? {};
    super(init.body, {});
    this.#app = `${init.app ?? ''}`;
    this.#method = `${init.method ?? ''}`;
  }

  get app() {
    return this.#app;
  }

  get method() {
    return this.#method;
  }
}

class ServiceResponse extends Response {}

class BindingRequest extends Body {
  #name;
  #metadata;
  #operation;
  constructor(init) {
    init = init ?? {};
    super(init.body, {});
    this.#name = `${init.name ?? ''}`;
    this.#metadata = init.metadata ? objectToKeyValueMap(init.metadata) : {};
    this.#operation = `${init.operation ?? ''}`;
  }

  get name() {
    return this.#name;
  }

  get metadata() {
    return { ...this.#metadata };
  }

  get operation() {
    return this.#operation;
  }
}

class BindingResponse extends Response {
  #metadata;
  #metadataParsed;

  constructor(body, init) {
    super(body, init);

    this.#metadata = init.metadata ?? '';
    this.#metadataParsed = false;
  }

  get metadata() {
    if (!this.#metadataParsed) {
      try {
        this.#metadata = JSON.parse(this.#metadata);
      } catch (e) {} finally {
        this.#metadataParsed = true;
      }
    }

    return this.#metadata;
  }
}

wrapper.mod = {
  ServiceRequest,
  ServiceResponse,
  BindingRequest,
  BindingResponse,
};
