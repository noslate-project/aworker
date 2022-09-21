/// <reference path="common.d.ts" />

declare namespace aworker {
  namespace zlib {
    type WindowBits = 0 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15;
    type Level = -1 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9;
    type MemLevel = 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9;

    /**
     * Zip type (`COMPRESS_TYPE.GZIP`, `COMPRESS_TYPE.DEFLATE`).
     */
    enum ZipType {
      GZIP = 0,
      DEFLATE = 1,
    }

    /**
     * Unzip type (`UNCOMPRESS_TYPE.GUNZIP`, `UNCOMPRESS_TYPE.INFLATE` and
     * `UNCOMPRESS_TYPE.UNZIP`).
     */
    enum UnzipType {
      GUNZIP = 0,
      INFLATE = 1,
      UNZIP = 2,
    }

    /**
     * Flush mode for compression and uncompression.
     */
    enum FlushMode {
      Z_NO_FLUSH = 0,
      Z_PARTIAL_FLUSH = 1,
      Z_SYNC_FLUSH = 2,
      Z_FULL_FLUSH = 3,
      Z_FINISH = 4,
      Z_BLOCK = 5,
      Z_TREES = 6,
    }

    /**
     * Stratagy for compression.
     */
    enum Stratagy {
      Z_DEFAULT_STRATEGY = 0,
      Z_FILTERED = 1,
      Z_HUFFMAN_ONLY = 2,
      Z_RLE = 3,
      Z_FIXED = 4,
    }

    /**
     * Unzip options.
     */
    interface UnzipOptions {
      /**
       * flush mode.
       */
      flush?: FlushMode;

      /**
       * window bits for compression / uncompression.
       */
      windowBits?: WindowBits;
    }

    /**
     * Zip options.
     */
    interface ZipOptions extends UnzipOptions {
      /**
       * compression level.
       */
      level?: Level;

      /**
       * compression mem level.
       */
      memLevel?: MemLevel;

      /**
       * compression strategy.
       */
      strategy?: Stratagy;
    }

    /**
     * Unzip class
     */
    export class Unzip extends TransformStream<BufferLike, ArrayBuffer> {
      /**
       * @param type The unzip type.
       * @param options The unzip options.
       */
      constructor(type: UnzipType, options?: UnzipOptions);
    }

    /**
     * Zip class
     */
    export class Zip extends TransformStream<BufferLike, ArrayBuffer> {
      /**
       * @param type The zip type.
       * @param options The zip options.
       */
      constructor(type: ZipType, options?: ZipOptions);
    }

    export const COMPRESS_TYPE = ZipType;
    export const UNCOMPRESS_TYPE = UnzipType;
    export const FLUSH_MODE = FlushMode;
    export const STRATEGY = Stratagy;
  }
}
