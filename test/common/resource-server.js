'use strict';

const http = require('http');

class ResourceServer {
  #server;
  #slowEmitLocked = false;
  #stats;

  constructor() {
    this.resetStats();
    // TODO: serve resource
    this.#server = http.createServer((req, res) => {
      this.#stats.requestCount[req.url] = (this.#stats.requestCount[req.url] ?? 0) + 1;

      res.setHeader('x-server', 'aworker-resource-server');
      res.setHeader('set-cookie', 'foobar');
      const url = new URL(req.url, 'http://fake');

      const setHeaders = (req.headers['x-set-header'] ?? '').split(' ').filter(it => it.trim()).map(it => it.trim().split('='));
      setHeaders.forEach(([ key, val ]) => {
        res.setHeader(key, val);
      });

      if (url.pathname === '/ping') {
        res.end('pong');
        return;
      }
      if (url.pathname === '/echo') {
        req.pipe(res);
        return;
      }
      if (url.pathname === '/dump') {
        req.setEncoding('utf8');
        let body = '';
        req.on('data', chunk => {
          body += chunk;
        });
        req.on('end', () => {
          res.end(JSON.stringify({
            method: req.method,
            url: req.url,
            headers: req.headers,
            body,
          }));
        });
        return;
      }
      if (url.pathname === '/status-echo') {
        let status = Number.parseInt(req.headers['x-status-echo'] ?? 400);
        if (!Number.isSafeInteger(status)) {
          status = 400;
        }
        res.statusCode = status;
        res.end();
        return;
      }
      if (url.pathname === '/slow-emit-exclusive') {
        if (this.#slowEmitLocked) {
          res.statusCode = 400;
          res.end();
          return;
        }
        this.#slowEmitLocked = true;
        // fallthrough
      }
      if (url.pathname === '/slow-emit-exclusive' || url.pathname === '/slow-emit') {
        let times = Number.parseInt(url.searchParams.get('times') ?? 10);
        if (!Number.isSafeInteger(times)) {
          times = 10;
        }
        const interval = setInterval(() => {
          times--;
          if (times <= 0) {
            clearInterval(interval);
            res.end();
            this.#slowEmitLocked = false;
            return;
          }
          res.write(`${times}\n`);
        }, 100);
        return;
      }
      if (url.pathname === '/black-hole') {
        return;
      }
      if (url.pathname === '/abort') {
        res.destroy();
        return;
      }
      if (url.pathname === '/stats') {
        res.end(JSON.stringify(this.#stats));
        this.resetStats();
        return;
      }
      res.statusCode = 400;
      res.end();
    });
  }

  start() {
    return new Promise((resolve, reject) => {
      this.#server.listen(30122, err => {
        if (err) {
          return reject(err);
        }
        resolve();
      });
    });
  }

  close() {
    return new Promise((resolve, reject) => {
      this.#server.close(err => {
        if (err) {
          return reject(err);
        }
        resolve();
      });
    });
  }

  unref() {
    this.#server.unref();
  }

  resetStats() {
    this.#stats = {
      requestCount: {},
    };
  }
}

module.exports = ResourceServer;

if (require.main === module) {
  new ResourceServer().start();
}
