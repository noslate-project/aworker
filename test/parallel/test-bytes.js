'use strict';

const str = [
  `Lorem ipsum dolor sit amet, consectetur adipiscing elit. Suspendisse vestibulum id lacus in malesuada. Vivamus consequat enim rutrum, elementum erat nec, condimentum orci. Praesent gravida felis sed felis consectetur, vel facilisis odio facilisis. Interdum et malesuada fames ac ante ipsum primis in faucibus. Sed vitae urna non eros malesuada tincidunt. Fusce eu convallis nulla. Proin suscipit risus at leo aliquet mollis. Pellentesque et pretium tellus, laoreet varius nulla. Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Nullam mollis odio ac neque dignissim convallis. Morbi placerat est ac commodo sagittis. In lacus odio, pretium nec porttitor ut, laoreet ac orci. Quisque vestibulum interdum ipsum, ac ullamcorper dolor. Sed blandit nunc id arcu mollis volutpat.

Suspendisse potenti. Nulla volutpat leo id ante suscipit, quis ultrices est pharetra. Donec auctor vel orci ut iaculis. Morbi dictum feugiat nulla at volutpat. Fusce ac vehicula velit. Nam id elementum nisl. Cras rutrum risus in dignissim accumsan.

Suspendisse massa nunc, volutpat in mollis vel, varius hendrerit lacus. Mauris suscipit faucibus sem, ut tristique ligula suscipit ac. Donec dapibus ipsum ac commodo tincidunt. Vestibulum sollicitudin pulvinar mi eget elementum. Vivamus dapibus elementum blandit. Vivamus bibendum odio vitae mauris aliquam pellentesque. Cras quis aliquet nisl. Quisque et ante risus. Pellentesque facilisis iaculis risus, sit amet consectetur libero. Integer et placerat dui, et scelerisque magna. Aenean malesuada ante ac erat vehicula, ac egestas massa ornare.

Praesent sit amet efficitur enim, at laoreet turpis. Sed vel arcu vitae enim pretium commodo. Vestibulum eu interdum dui, in lacinia eros. Aliquam felis diam, pellentesque ac metus ac, accumsan ultrices libero. Cras elementum dolor tellus, sed rutrum mauris iaculis at. Mauris eget lorem nisl. Nam varius posuere iaculis. Nunc posuere arcu vel semper vulputate.

Proin vestibulum, urna quis scelerisque sagittis, orci ipsum convallis purus, id pretium ex neque a diam. Duis dignissim enim in lacus consectetur sollicitudin. Nullam pellentesque metus justo, vel cursus dui rutrum quis. Cras varius purus sit amet tellus interdum pulvinar. Duis eu mi nec massa aliquet aliquet. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Sed quis ligula ac eros lobortis vestibulum. Nam ac porttitor dui. Donec et feugiat lectus. Morbi vitae feugiat metus, in fermentum augue. Donec dapibus ante ac ligula hendrerit laoreet. Vestibulum porttitor orci eu turpis volutpat, ut porta elit tristique.

`,
  'aaaaaaaaaaaaaaaaaa',
];

const encoded = [
  'TG9yZW0gaXBzdW0gZG9sb3Igc2l0IGFtZXQsIGNvbnNlY3RldHVyIGFkaXBpc2NpbmcgZWxpdC4gU3VzcGVuZGlzc2UgdmVzdGlidWx1bSBpZCBsYWN1cyBpbiBtYWxlc3VhZGEuIFZpdmFtdXMgY29uc2VxdWF0IGVuaW0gcnV0cnVtLCBlbGVtZW50dW0gZXJhdCBuZWMsIGNvbmRpbWVudHVtIG9yY2kuIFByYWVzZW50IGdyYXZpZGEgZmVsaXMgc2VkIGZlbGlzIGNvbnNlY3RldHVyLCB2ZWwgZmFjaWxpc2lzIG9kaW8gZmFjaWxpc2lzLiBJbnRlcmR1bSBldCBtYWxlc3VhZGEgZmFtZXMgYWMgYW50ZSBpcHN1bSBwcmltaXMgaW4gZmF1Y2lidXMuIFNlZCB2aXRhZSB1cm5hIG5vbiBlcm9zIG1hbGVzdWFkYSB0aW5jaWR1bnQuIEZ1c2NlIGV1IGNvbnZhbGxpcyBudWxsYS4gUHJvaW4gc3VzY2lwaXQgcmlzdXMgYXQgbGVvIGFsaXF1ZXQgbW9sbGlzLiBQZWxsZW50ZXNxdWUgZXQgcHJldGl1bSB0ZWxsdXMsIGxhb3JlZXQgdmFyaXVzIG51bGxhLiBPcmNpIHZhcml1cyBuYXRvcXVlIHBlbmF0aWJ1cyBldCBtYWduaXMgZGlzIHBhcnR1cmllbnQgbW9udGVzLCBuYXNjZXR1ciByaWRpY3VsdXMgbXVzLiBOdWxsYW0gbW9sbGlzIG9kaW8gYWMgbmVxdWUgZGlnbmlzc2ltIGNvbnZhbGxpcy4gTW9yYmkgcGxhY2VyYXQgZXN0IGFjIGNvbW1vZG8gc2FnaXR0aXMuIEluIGxhY3VzIG9kaW8sIHByZXRpdW0gbmVjIHBvcnR0aXRvciB1dCwgbGFvcmVldCBhYyBvcmNpLiBRdWlzcXVlIHZlc3RpYnVsdW0gaW50ZXJkdW0gaXBzdW0sIGFjIHVsbGFtY29ycGVyIGRvbG9yLiBTZWQgYmxhbmRpdCBudW5jIGlkIGFyY3UgbW9sbGlzIHZvbHV0cGF0LgoKU3VzcGVuZGlzc2UgcG90ZW50aS4gTnVsbGEgdm9sdXRwYXQgbGVvIGlkIGFudGUgc3VzY2lwaXQsIHF1aXMgdWx0cmljZXMgZXN0IHBoYXJldHJhLiBEb25lYyBhdWN0b3IgdmVsIG9yY2kgdXQgaWFjdWxpcy4gTW9yYmkgZGljdHVtIGZldWdpYXQgbnVsbGEgYXQgdm9sdXRwYXQuIEZ1c2NlIGFjIHZlaGljdWxhIHZlbGl0LiBOYW0gaWQgZWxlbWVudHVtIG5pc2wuIENyYXMgcnV0cnVtIHJpc3VzIGluIGRpZ25pc3NpbSBhY2N1bXNhbi4KClN1c3BlbmRpc3NlIG1hc3NhIG51bmMsIHZvbHV0cGF0IGluIG1vbGxpcyB2ZWwsIHZhcml1cyBoZW5kcmVyaXQgbGFjdXMuIE1hdXJpcyBzdXNjaXBpdCBmYXVjaWJ1cyBzZW0sIHV0IHRyaXN0aXF1ZSBsaWd1bGEgc3VzY2lwaXQgYWMuIERvbmVjIGRhcGlidXMgaXBzdW0gYWMgY29tbW9kbyB0aW5jaWR1bnQuIFZlc3RpYnVsdW0gc29sbGljaXR1ZGluIHB1bHZpbmFyIG1pIGVnZXQgZWxlbWVudHVtLiBWaXZhbXVzIGRhcGlidXMgZWxlbWVudHVtIGJsYW5kaXQuIFZpdmFtdXMgYmliZW5kdW0gb2RpbyB2aXRhZSBtYXVyaXMgYWxpcXVhbSBwZWxsZW50ZXNxdWUuIENyYXMgcXVpcyBhbGlxdWV0IG5pc2wuIFF1aXNxdWUgZXQgYW50ZSByaXN1cy4gUGVsbGVudGVzcXVlIGZhY2lsaXNpcyBpYWN1bGlzIHJpc3VzLCBzaXQgYW1ldCBjb25zZWN0ZXR1ciBsaWJlcm8uIEludGVnZXIgZXQgcGxhY2VyYXQgZHVpLCBldCBzY2VsZXJpc3F1ZSBtYWduYS4gQWVuZWFuIG1hbGVzdWFkYSBhbnRlIGFjIGVyYXQgdmVoaWN1bGEsIGFjIGVnZXN0YXMgbWFzc2Egb3JuYXJlLgoKUHJhZXNlbnQgc2l0IGFtZXQgZWZmaWNpdHVyIGVuaW0sIGF0IGxhb3JlZXQgdHVycGlzLiBTZWQgdmVsIGFyY3Ugdml0YWUgZW5pbSBwcmV0aXVtIGNvbW1vZG8uIFZlc3RpYnVsdW0gZXUgaW50ZXJkdW0gZHVpLCBpbiBsYWNpbmlhIGVyb3MuIEFsaXF1YW0gZmVsaXMgZGlhbSwgcGVsbGVudGVzcXVlIGFjIG1ldHVzIGFjLCBhY2N1bXNhbiB1bHRyaWNlcyBsaWJlcm8uIENyYXMgZWxlbWVudHVtIGRvbG9yIHRlbGx1cywgc2VkIHJ1dHJ1bSBtYXVyaXMgaWFjdWxpcyBhdC4gTWF1cmlzIGVnZXQgbG9yZW0gbmlzbC4gTmFtIHZhcml1cyBwb3N1ZXJlIGlhY3VsaXMuIE51bmMgcG9zdWVyZSBhcmN1IHZlbCBzZW1wZXIgdnVscHV0YXRlLgoKUHJvaW4gdmVzdGlidWx1bSwgdXJuYSBxdWlzIHNjZWxlcmlzcXVlIHNhZ2l0dGlzLCBvcmNpIGlwc3VtIGNvbnZhbGxpcyBwdXJ1cywgaWQgcHJldGl1bSBleCBuZXF1ZSBhIGRpYW0uIER1aXMgZGlnbmlzc2ltIGVuaW0gaW4gbGFjdXMgY29uc2VjdGV0dXIgc29sbGljaXR1ZGluLiBOdWxsYW0gcGVsbGVudGVzcXVlIG1ldHVzIGp1c3RvLCB2ZWwgY3Vyc3VzIGR1aSBydXRydW0gcXVpcy4gQ3JhcyB2YXJpdXMgcHVydXMgc2l0IGFtZXQgdGVsbHVzIGludGVyZHVtIHB1bHZpbmFyLiBEdWlzIGV1IG1pIG5lYyBtYXNzYSBhbGlxdWV0IGFsaXF1ZXQuIFBlbGxlbnRlc3F1ZSBoYWJpdGFudCBtb3JiaSB0cmlzdGlxdWUgc2VuZWN0dXMgZXQgbmV0dXMgZXQgbWFsZXN1YWRhIGZhbWVzIGFjIHR1cnBpcyBlZ2VzdGFzLiBTZWQgcXVpcyBsaWd1bGEgYWMgZXJvcyBsb2JvcnRpcyB2ZXN0aWJ1bHVtLiBOYW0gYWMgcG9ydHRpdG9yIGR1aS4gRG9uZWMgZXQgZmV1Z2lhdCBsZWN0dXMuIE1vcmJpIHZpdGFlIGZldWdpYXQgbWV0dXMsIGluIGZlcm1lbnR1bSBhdWd1ZS4gRG9uZWMgZGFwaWJ1cyBhbnRlIGFjIGxpZ3VsYSBoZW5kcmVyaXQgbGFvcmVldC4gVmVzdGlidWx1bSBwb3J0dGl0b3Igb3JjaSBldSB0dXJwaXMgdm9sdXRwYXQsIHV0IHBvcnRhIGVsaXQgdHJpc3RpcXVlLgoK',
  'YWFhYWFhYWFhYWFhYWFhYWFh',
];

test(() => {
  for (let i = 0; i < str.length; i++) {
    assert_equals(btoa(str[i]), encoded[i]);
  }
}, 'btoa long string');

test(() => {
  for (let i = 0; i < str.length; i++) {
    assert_equals(atob(encoded[i]), str[i]);
  }
}, 'atob long string');

test(() => {
  assert_throws_exactly('The string to be decoded is not correctly encoded.', () => {
    try {
      atob('斯大林看风景');
    } catch (e) {
      throw e.message;
    }
  });
}, 'atob failed');
