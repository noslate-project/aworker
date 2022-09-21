'use strict';

const { URL } = load('url');

// https://html.spec.whatwg.org/multipage/history.html#the-location-interface
class Location {
  #href;
  #origin;
  #protocol;
  #host;
  #hostname;
  #port;
  #pathname;
  #search;
  #hash;
  constructor(href) {
    const url = new URL(href);
    this.#href = url.href;
    this.#origin = url.origin;
    this.#protocol = url.protocol;
    this.#host = url.host;
    this.#hostname = url.hostname;
    this.#port = url.port;
    this.#pathname = url.pathname;
    this.#search = url.search;
    this.#hash = url.hash;
  }

  get href() {
    return this.#href;
  }

  get origin() {
    return this.#origin;
  }

  get protocol() {
    return this.#protocol;
  }

  get host() {
    return this.#host;
  }

  get hostname() {
    return this.#hostname;
  }

  get port() {
    return this.#port;
  }

  get pathname() {
    return this.#pathname;
  }

  get search() {
    return this.#search;
  }

  get hash() {
    return this.#hash;
  }

  // https://html.spec.whatwg.org/multipage/history.html#dom-location-assign
  assign() {
    /** do nothing */
  }

  // https://html.spec.whatwg.org/multipage/history.html#dom-location-replace
  replace() {
    /** do nothing */
  }

  // https://html.spec.whatwg.org/multipage/history.html#dom-location-reload
  reload() {
    /** do nothing */
  }
}

wrapper.mod = {
  Location,
};
