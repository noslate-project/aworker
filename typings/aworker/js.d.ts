declare namespace aworker {
  namespace js {
    interface ContextOptions {
      /**
       * The context name.
       */
      name?: string;
      /**
       * The context origin. (useless so far)
       */
      origin?: string;
    }

    interface ScriptOptions {
      /**
       * Dummy script filename.
       */
      filename?: string;

      /**
       * Line offset.
       */
      lineOffset?: number;

      /**
       * Column offset.
       */
      columnOffset?: number;

      /**
       * Cached data.
       */
      cachedData?: Uint8Array;
    }

    interface ExecuteOptions {
      /**
       * The execution timeout. (0 indicates for no timeout)
       */
      timeout?: number;
    }

    /**
     * The Script class to be executed in a context.
     */
    export class Script {
      /**
       * @param code The source code string.
       * @param options The script options. (defaults to `{ filename: 'aworker.js.<anonymous>' }`)
       */
      constructor(code: string, options?: ScriptOptions);

      /**
       * Script filename.
       */
      readonly filename: string;

      /**
       * Line offset.
       */
      readonly lineOffset: number;

      /**
       * Column offset.
       */
      readonly columnOffset: number;

      /**
       * If the cached data is consumed by v8.
       */
      readonly cachedDataConsumed: boolean;

      /**
       * Create a cached data. Can be used to create a Script.
       */
      createCachedData(): Uint8Array;
    }

    /**
     * The Context class to execute a script.
     */
    export class Context {
      /**
       * @param sandbox A non-null sandbox object.
       * @param options The context options.
       */
      constructor(sandbox: Record<string, unknown>, options?: ContextOptions);

      /**
       * Execute a script and return it's last statement's result.
       * @param script The script string or script object to be executed.
       * @param options The script options.
       * @returns {any} The last statement's result of the script.
       */
      execute(script: string|Script, options?: ExecuteOptions): any;

      /**
       * The context name.
       */
      readonly name: string;

      /**
       * The `globalThis` object for this context. Deeply equals to `sandbox` in `constructor`.
       */
      get globalThis(): Record<string, unknown>;
    }
  }
}
