export async function foo() {
  await import ('./b.mjs');
  await (await import ('./c.mjs')).foo();
  return 'bar';
}
