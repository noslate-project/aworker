declare class HashOrHmac {
  /**
   * Update hash / hmac input.
   * @param data The input data.
   * @returns {number} Not that important.
   */
  update(data: aworker.crypto.CryptoInputType): number;

  /**
   * Get the final result.
   * @returns {HashFinalResult} The final result.
   */
  final(): aworker.crypto.HashFinalResult;
}

declare namespace aworker {
  namespace crypto {
    /**
     * The CryptoInputType only supports TypedArray / ArrayBuffer and string.
     */
    type CryptoInputType = Uint8Array | Uint16Array | Uint32Array | ArrayBuffer | string;

    /**
     * An enum that for `hashFinalResult.digest()`.
     */
    enum CryptoDigestType {
      typedArray = 'typedArray',
      utf8 = 'utf8',
      'utf-8' = 'utf-8',
      hex = 'hex',
      base64 = 'base64',
      binary = 'binary',
    }

    /**
     * Supported hash / hmac algorithm.
     */
    enum HashAlgorithm {
      md5 = 'md5',
      sha1 = 'sha1',
      sha224 = 'sha224',
      sha256 = 'sha256',
      sha512 = 'sha512',
    }

    /**
     * Use its `result.digest()` method please.
     */
    interface HashFinalResult {
      /**
       * Transform hash final result to `string` or `Uint8Array`.
       * @param type The digest type. Defaults to `'typedArray'`.
       * @returns {string | Uint8Array} returns a `Uint8Array` if `type` is `'binary'` or `'typedArray'`; otherwise returns a string.
       */
      digest(type?: CryptoDigestType): string | Uint8Array;
    }

    /**
     * Class for calculating Buffer / string's hash.
     */
    export class Hash extends HashOrHmac {
      /**
       * Hash constructor.
       * @param algo The hash algorithm.
       */
      constructor(algo: HashAlgorithm);
    }

    /**
     * Class for calculating Buffer / string's hmac.
     */
    export class Hmac extends HashOrHmac {
      /**
       * Hmac constructor.
       * @param algo The hash algorithm.
       */
      constructor(algo: HashAlgorithm, key: CryptoInputType);
    }
  }
}
