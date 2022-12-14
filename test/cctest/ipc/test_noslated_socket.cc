#include <thread>  // NOLINT(build/c++11)
#include "aworker_logger.h"
#include "gtest/gtest.h"
#include "ipc/ipc_delegate_impl.h"
#include "ipc/ipc_service.h"
#include "ipc/ipc_socket_server.h"
#include "ipc/ipc_socket_uv.h"
#include "util.h"

using std::make_shared;
using std::shared_ptr;
using std::unique_ptr;

namespace aworker {
namespace ipc {
namespace {
class NoslatedServer : public NoslatedService {
 private:
  static void AsyncCb(uv_async_t* handle) {
    NoslatedServer* self = ContainerOf(&NoslatedServer::stop_, handle);
    uv_close(reinterpret_cast<uv_handle_t*>(handle), CloseCb);
    self->server_->Close();
  }
  static void CloseCb(uv_handle_t* handle) {}

 public:
  void Start(std::string server_socket_path) {
    uv_loop_init(&loop_);
    uv_async_init(&loop_, &stop_, AsyncCb);
    server_ = new server::NoslatedSocketServer(
        unowned_ptr(&loop_), server_socket_path, unowned_ptr(this));
    server_->Initialize();
    work_thread_ = std::thread([this]() { uv_run(&loop_, UV_RUN_DEFAULT); });
  }

  void Stop() {
    uv_async_send(&stop_);
    work_thread_.join();
    uv_loop_close(&loop_);
  }

  void Credentials(unique_ptr<RpcController> controller,
                   const unique_ptr<CredentialsRequestMessage> req,
                   unique_ptr<CredentialsResponseMessage> response,
                   Closure<CredentialsResponseMessage> closure) override {
    ILOG("test server on credential request");
    if (req->cred() == "foobar") {
      closure(CanonicalCode::OK, nullptr, std::move(response));
      return;
    }
    closure(CanonicalCode::CLIENT_ERROR, nullptr, nullptr);
  };

  void Disconnected(SessionId) override{};

 protected:
  std::weak_ptr<SocketDelegate> socket_delegate(SessionId session_id) override {
    if (auto it = server_->Session(session_id).lock()) {
      return it->socket()->delegate();
    }
    return std::weak_ptr<SocketDelegate>();
  }

 private:
  std::thread work_thread_;
  uv_loop_t loop_;
  uv_async_t stop_;
  server::NoslatedSocketServer* server_;
};

namespace uv {
class NoslatedClient : public NoslatedService {
 public:
  class ClientDelegate : public DelegateImpl {
   public:
    ClientDelegate(shared_ptr<NoslatedClient> client,
                   shared_ptr<EventLoop> loop)
        : DelegateImpl(client, loop), client_(client) {}
    std::shared_ptr<SocketHolder> socket() override {
      if (client_->socket_ == nullptr) {
        return nullptr;
      }
      return unowned_ptr(client_->socket_.get());
    }

    void OnError() override { client_->OnError(); };

   private:
    shared_ptr<NoslatedClient> client_;
  };
  explicit NoslatedClient(shared_ptr<UvLoop> loop) {
    delegate_ = std::make_shared<ClientDelegate>(
        unowned_ptr(this), std::static_pointer_cast<EventLoop>(loop));
  }

  void Disconnected(SessionId) override { ILOG("disconnected"); };

  void set_socket(UvSocketHolder::Pointer socket) {
    socket_ = std::move(socket);
  }

  shared_ptr<SocketDelegate> delegate() { return delegate_; }

  void Send() {
    auto req = std::make_unique<CredentialsRequestMessage>();
    req->set_type(CredentialTargetType::Data);
    req->set_cred("foobar");
    Request(NewControllerWithTimeout(1000),
            std::move(req),
            [this](CanonicalCode code,
                   unique_ptr<ErrorResponseMessage> error,
                   unique_ptr<CredentialsResponseMessage> resp) {
              CHECK_EQ(code, CanonicalCode::OK);
              socket_.reset();
            });
  }

 protected:
  std::weak_ptr<SocketDelegate> socket_delegate(SessionId session_id) override {
    return std::weak_ptr<SocketDelegate>(delegate_);
  };

 private:
  void OnError() {
    ILOG("on error");
    socket_.reset();
  }
  shared_ptr<ClientDelegate> delegate_;
  UvSocketHolder::Pointer socket_;
};
}  // namespace uv

TEST(NoslatedSocketUvTest, Main) {
  char server_path[PATH_MAX];
  realpath("/tmp/.noslated.sock", server_path);
  NoslatedServer server;
  server.Start(server_path);

  uv_loop_t client_loop;
  uv_loop_init(&client_loop);

  std::shared_ptr<uv::NoslatedClient> client =
      std::make_shared<uv::NoslatedClient>(make_shared<UvLoop>(&client_loop));
  UvSocketHolder::Connect(make_shared<UvLoop>(&client_loop),
                          server_path,
                          client->delegate(),
                          [client](UvSocketHolder::Pointer socket) {
                            client->set_socket(std::move(socket));
                            client->Send();
                          });
  uv_run(&client_loop, UV_RUN_DEFAULT);
  uv_loop_close(&client_loop);
  server.Stop();
}

}  // namespace
}  // namespace ipc
}  // namespace aworker
