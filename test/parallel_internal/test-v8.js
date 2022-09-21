'use strict';

const v8 = load('v8');

test(() => {
  v8.writeHeapSnapshot('./foobar.heapsnapshot');
  v8.writeHeapSnapshot('./foobar1.heapsnapshot');
  v8.writeHeapSnapshot('./foobar2.heapsnapshot');
}, 'Consecutively writing heap snapshots');
