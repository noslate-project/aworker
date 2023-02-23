'use strict';

const { createBaggages } = load('service_worker/baggages');

test(() => {
  const baggages = createBaggages([[ 'foo', 'bar' ]]);
  assert_equals(baggages.get('foo'), 'bar');
}, 'should construct with kv pairs');

test(() => {
  const baggages = createBaggages({
    foo: 'bar',
  });
  assert_equals(baggages.get('foo'), 'bar');
}, 'should construct with a dict');

test(() => {
  const baggages = createBaggages({
    foo: 'bar',
  });
  baggages.set('foo', 'quz');
  assert_equals(baggages.get('foo'), 'quz');
}, 'should update value');

test(() => {
  const baggages = createBaggages({
    foo: 'bar',
    quz: 'quk',
  });
  baggages.delete('foo');
  assert_false(baggages.has('foo'));
}, 'should delete entry');
