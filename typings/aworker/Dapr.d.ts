declare namespace aworker {
  namespace Dapr {
    type DaprBodyType = Uint8Array | ArrayBuffer | string | ReadableStream | FormData | URLSearchParams | null;
    interface ServiceRequestInit {
      body?: DaprBodyType;
      app?: string;
      method?: string;
    }

    interface BindingRequestInit {
      body?: DaprBodyType;
      name?: string;
      operation?: string;
      metadata?: object;
    }

    interface ServiceRequest extends Body {
      new(init?: ServiceRequestInit): ServiceRequest;
      readonly app: string;
      readonly method: string;
    }

    interface ServiceResponse extends Response {
      new(init?: DaprBodyType): ServiceResponse;
    }

    interface BindingRequest extends Body {
      new(init?: BindingRequestInit): BindingRequest;
      readonly name: string;
      readonly operation: string;
      readonly metadata: object;
    }

    interface BindingResponse extends Response {
      new(init?: DaprBodyType): ServiceResponse;
    }

    interface DaprV1 {
      invoke(init?: ServiceRequest | ServiceRequestInit): Promise<ServiceResponse>;
      binding(init?: BindingRequest | BindingRequestInit): Promise<BindingResponse>;
    }
  }

  interface DaprNamespace {
    ['1.0']: Dapr.DaprV1;
  }

  const Dapr: DaprNamespace;
}
