declare namespace aworker {
  type Func<T> = (...args: unknown[]) => T;

  class AsyncLocalStorage<T> {
    disable(): void;
    run<R>(store: T, callback: Func<R>, ...args: unknown[]): R;
    getStore(): T | undefined;
  }
}
