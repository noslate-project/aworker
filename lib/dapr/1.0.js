'use strict';

const agentChannel = load('agent_channel');
const { ServiceRequest, ServiceResponse, BindingRequest, BindingResponse } = load('dapr/common');

class DaprV1$0 {
  ServiceRequest = ServiceRequest;
  ServiceResponse = ServiceResponse;
  BindingRequest = BindingRequest;
  BindingResponse = BindingResponse;
  async invoke(init) {
    if (!(init instanceof ServiceRequest)) {
      init = new ServiceRequest(init);
    }
    const buffer = await init.arrayBuffer();
    const result = await agentChannel.call('daprInvoke', {
      app: init.app,
      method: init.method,
      body: new Uint8Array(buffer),
    });
    const response = new ServiceResponse(result.body, { status: result.status });
    return response;
  }

  async binding(init) {
    if (!(init instanceof BindingRequest)) {
      init = new BindingRequest(init);
    }
    const buffer = await init.arrayBuffer();
    const result = await agentChannel.call('daprBinding', {
      name: init.name,
      metadata: init.metadata,
      operation: init.operation,
      body: new Uint8Array(buffer),
    });

    const response = new BindingResponse(result.body, { status: result.status, metadata: result.metadata });
    return response;
  }
}

wrapper.mod = DaprV1$0;
