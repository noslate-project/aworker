'use strict';

const nameToCodeMap = new Map();
forEachCode((name, codeName, value) => {
  nameToCodeMap.set(name, value);
});

class DOMException extends Error {
  #name;
  #message;
  constructor(message = '', name = 'Error') {
    super();
    this.#message = message;
    this.#name = name;
  }

  get message() {
    return this.#message;
  }

  get name() {
    return this.#name;
  }

  get code() {
    const code = nameToCodeMap.get(this.#name);
    return code === undefined ? 0 : code;
  }
}

function forEachCode(fn) {
  fn('IndexSizeError', 'INDEX_SIZE_ERR', 1);
  fn('DOMStringSizeError', 'DOMSTRING_SIZE_ERR', 2);
  fn('HierarchyRequestError', 'HIERARCHY_REQUEST_ERR', 3);
  fn('WrongDocumentError', 'WRONG_DOCUMENT_ERR', 4);
  fn('InvalidCharacterError', 'INVALID_CHARACTER_ERR', 5);
  fn('NoDataAllowedError', 'NO_DATA_ALLOWED_ERR', 6);
  fn('NoModificationAllowedError', 'NO_MODIFICATION_ALLOWED_ERR', 7);
  fn('NotFoundError', 'NOT_FOUND_ERR', 8);
  fn('NotSupportedError', 'NOT_SUPPORTED_ERR', 9);
  fn('InUseAttributeError', 'INUSE_ATTRIBUTE_ERR', 10);
  fn('InvalidStateError', 'INVALID_STATE_ERR', 11);
  fn('SyntaxError', 'SYNTAX_ERR', 12);
  fn('InvalidModificationError', 'INVALID_MODIFICATION_ERR', 13);
  fn('NamespaceError', 'NAMESPACE_ERR', 14);
  fn('InvalidAccessError', 'INVALID_ACCESS_ERR', 15);
  fn('ValidationError', 'VALIDATION_ERR', 16);
  fn('TypeMismatchError', 'TYPE_MISMATCH_ERR', 17);
  fn('SecurityError', 'SECURITY_ERR', 18);
  fn('NetworkError', 'NETWORK_ERR', 19);
  fn('AbortError', 'ABORT_ERR', 20);
  fn('URLMismatchError', 'URL_MISMATCH_ERR', 21);
  fn('QuotaExceededError', 'QUOTA_EXCEEDED_ERR', 22);
  fn('TimeoutError', 'TIMEOUT_ERR', 23);
  fn('InvalidNodeTypeError', 'INVALID_NODE_TYPE_ERR', 24);
  fn('DataCloneError', 'DATA_CLONE_ERR', 25);
  // There are some more error names, but since they don't have codes assigned,
  // we don't need to care about them.
}

forEachCode((name, codeName, value) => {
  const desc = { enumerable: true, value };
  Object.defineProperty(DOMException, codeName, desc);
  Object.defineProperty(DOMException.prototype, codeName, desc);
});

function createInvalidStateError(message) {
  return new DOMException(message ?? 'The object is in an invalid state.', 'InvalidStateError');
}

function createAbortError(message) {
  return new DOMException(message ?? 'Operation aborted.', 'AbortError');
}

function createQuotaExceededError(message) {
  return new DOMException(message ?? 'Quota exceeded limit.', 'QuotaExceededError');
}

wrapper.mod = {
  DOMException,

  createAbortError,
  createInvalidStateError,
  createQuotaExceededError,
};
