'use strict';

const { getHeapSnapshot } = load('v8');

test(() => {
  const jsonStr = getHeapSnapshot();
  const snapshot = JSON.parse(jsonStr);
  assert_equals(typeof snapshot, 'object');
}, 'getHeapSnapshot should return valid JSON');

test(() => {
  validateSnapshot('Aworker / Immortal', [{
    children: [
      // the main script
      { node_name: 'Aworker / script_wrap' },
    ],
  }]);
}, 'verify Immortal nodes');

// Validate the v8 heap snapshot
function validateSnapshot(rootName, expected, { loose = false } = {}) {
  const nodes = createJSHeapSnapshot();

  const rootNodes = nodes.filter(
    node => node.name === rootName && node.type !== 'string');
  if (loose) {
    assert_true(rootNodes.length >= expected.length,
      `Expect to find at least ${expected.length} '${rootName}'`);
  } else {
    assert_equals(
      rootNodes.length, expected.length,
      `Expect to find ${expected.length} '${rootName}'`);
  }

  for (const expectation of expected) {
    if (expectation.children) {
      for (const expectedEdge of expectation.children) {
        const check = edge => isExpectedEdge(edge, expectedEdge);
        const hasChild = rootNodes.some(
          node => node.outgoingEdges.some(check)
        );
        assert_true(hasChild, `expected to find child ${JSON.stringify(expectedEdge)}`);
      }
    }
  }
}

function isExpectedEdge(edge, { node_name, edge_name }) {
  if (edge_name !== undefined && edge.name !== edge_name) {
    return false;
  }
  // From our internal embedded graph
  if (edge.to.value) {
    if (edge.to.value.constructor.name !== node_name) {
      return false;
    }
  } else if (edge.to.name !== node_name) {
    return false;
  }
  return true;
}

function createJSHeapSnapshot() {
  const dump = JSON.parse(getHeapSnapshot());
  assert_equals(typeof dump, 'object');
  const meta = dump.snapshot.meta;

  const nodes =
    readHeapInfo(dump.nodes, meta.node_fields, meta.node_types, dump.strings);
  const edges =
    readHeapInfo(dump.edges, meta.edge_fields, meta.edge_types, dump.strings);

  for (const node of nodes) {
    node.incomingEdges = [];
    node.outgoingEdges = [];
  }

  let fromNodeIndex = 0;
  let edgeIndex = 0;
  for (const { type, name_or_index, to_node } of edges) {
    while (edgeIndex === nodes[fromNodeIndex].edge_count) {
      edgeIndex = 0;
      fromNodeIndex++;
    }
    const toNode = nodes[to_node / meta.node_fields.length];
    const fromNode = nodes[fromNodeIndex];
    const edge = {
      type,
      to: toNode,
      from: fromNode,
      name: typeof name_or_index === 'string' ? name_or_index : null,
    };
    toNode.incomingEdges.push(edge);
    fromNode.outgoingEdges.push(edge);
    edgeIndex++;
  }

  for (const node of nodes) {
    assert_equals(node.edge_count, node.outgoingEdges.length);
  }
  return nodes;
}

function readHeapInfo(raw, fields, types, strings) {
  const items = [];

  for (let i = 0; i < raw.length; i += fields.length) {
    const item = {};
    for (let j = 0; j < fields.length; j++) {
      const name = fields[j];
      let type = types[j];
      if (Array.isArray(type)) {
        item[name] = type[raw[i + j]];
      } else if (name === 'name_or_index') { // type === 'string_or_number'
        if (item.type === 'element' || item.type === 'hidden') {
          type = 'number';
        } else {
          type = 'string';
        }
      }

      if (type === 'string') {
        item[name] = strings[raw[i + j]];
      } else if (type === 'number' || type === 'node') {
        item[name] = raw[i + j];
      }
    }
    items.push(item);
  }

  return items;
}
