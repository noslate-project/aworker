declare namespace aworker {
  type Platform =
    | 'darwin'
    | 'linux'
    | 'win32';

  export const env: Record<string, string | undefined>;
  export const arch: string;
  export const platform: Platform;
  export const version: string;

  /**
   * The `aworker.exit()` method instructs Serverless Worker to terminate the
   * process synchronously with an exit status of code. If `code` is omitted,
   * exit uses the 'success' code 0.
   * @param code - The exit code.
   */
  export function exit(code?: number): never;
}
