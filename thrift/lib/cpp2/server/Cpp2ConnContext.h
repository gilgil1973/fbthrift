/*
 * Copyright 2014 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THRIFT_ASYNC_CPP2CONNCONTEXT_H_
#define THRIFT_ASYNC_CPP2CONNCONTEXT_H_ 1

#include <thrift/lib/cpp/async/TAsyncSocket.h>
#include <thrift/lib/cpp/server/TConnectionContext.h>
#include <thrift/lib/cpp/concurrency/ThreadManager.h>
#include <thrift/lib/cpp/transport/THeader.h>
#include <thrift/lib/cpp/transport/TSocketAddress.h>
#include <thrift/lib/cpp2/async/SaslServer.h>
#include <thrift/lib/cpp2/async/HeaderClientChannel.h>

#include <memory>

using apache::thrift::concurrency::PriorityThreadManager;

namespace apache { namespace thrift {

class Cpp2ConnContext : public apache::thrift::server::TConnectionContext {
 public:
  explicit Cpp2ConnContext(
    const folly::SocketAddress* address,
    const apache::thrift::async::TAsyncSocket* socket,
    apache::thrift::transport::THeader* header,
    const apache::thrift::SaslServer* sasl_server,
    apache::thrift::async::TEventBaseManager* manager,
    const std::shared_ptr<HeaderClientChannel>& duplexChannel = nullptr)
    : peerAddress_(*address),
      header_(header),
      saslServer_(sasl_server),
      manager_(manager),
      duplexChannel_(duplexChannel) {
    if (socket) {
      socket->getLocalAddress(&localAddress_);
    }
  }

  virtual const folly::SocketAddress*
  getPeerAddress() const {
    return &peerAddress_;
  }

  const folly::SocketAddress* getLocalAddress() const {
    return &localAddress_;
  }

  void reset() {
    peerAddress_.reset();
    localAddress_.reset();
    header_ = nullptr;
    cleanupUserData();
  }

  /**
   * These are not useful in Cpp2: Header data is contained in
   * Cpp2Request below, and protocol itself is not instantiated
   * until we are in the generated code.
   */
  virtual std::shared_ptr<apache::thrift::protocol::TProtocol>
  getInputProtocol() const {
    return std::shared_ptr<apache::thrift::protocol::TProtocol>();
  }
  virtual std::shared_ptr<apache::thrift::protocol::TProtocol>
  getOutputProtocol() const {
    return std::shared_ptr<apache::thrift::protocol::TProtocol>();
  }

  virtual apache::thrift::transport::THeader* getHeader() {
    return header_;
  }

  virtual void setSaslServer(const apache::thrift::SaslServer* sasl_server) {
    saslServer_ = sasl_server;
  }

  virtual const apache::thrift::SaslServer* getSaslServer() const {
    return saslServer_;
  }

  virtual apache::thrift::async::TEventBaseManager* getEventBaseManager() {
    return manager_;
  }

  template <typename Client>
  Client* getDuplexClient() {
    DCHECK(duplexChannel_);
    Client* client = dynamic_cast<Client*>(duplexClient_.get());
    if (!client) {
      client = new Client(duplexChannel_);
      duplexClient_.reset(client);
    }
    return client;
  }
 private:
  folly::SocketAddress peerAddress_;
  folly::SocketAddress localAddress_;
  apache::thrift::transport::THeader* header_;
  const apache::thrift::SaslServer* saslServer_;
  apache::thrift::async::TEventBaseManager* manager_;
  std::shared_ptr<HeaderClientChannel> duplexChannel_;
  std::unique_ptr<TClientBase> duplexClient_;
};

// Request-specific context
class Cpp2RequestContext : public apache::thrift::server::TConnectionContext {
 public:
  explicit Cpp2RequestContext(Cpp2ConnContext* ctx)
      : ctx_(ctx) {
    setConnectionContext(ctx);
  }

  void setConnectionContext(Cpp2ConnContext* ctx) {
    ctx_ = ctx;
    if (ctx_) {
      auto header = ctx_->getHeader();
      if (header) {
        headers_ = header->getHeaders();
        transforms_ = header->getWriteTransforms();
        minCompressBytes_ = header->getMinCompressBytes();
        callPriority_ = header->getCallPriority();
      }
    }
  }

  // Forward all connection-specific information
  virtual const folly::SocketAddress*
  getPeerAddress() const {
    return ctx_->getPeerAddress();
  }

  const folly::SocketAddress* getLocalAddress() const {
    return ctx_->getLocalAddress();
  }

  void reset() {
    ctx_->reset();
  }

  virtual std::shared_ptr<apache::thrift::protocol::TProtocol>
  getInputProtocol() const {
    return ctx_->getInputProtocol();
  }

  virtual std::shared_ptr<apache::thrift::protocol::TProtocol>
  getOutputProtocol() const {
    return ctx_->getOutputProtocol();
  }

  // The following two header functions _are_ thread safe
  virtual std::map<std::string, std::string> getHeaders() {
    return headers_;
  }

  virtual std::map<std::string, std::string> getWriteHeaders() {
    return std::move(writeHeaders_);
  }

  std::map<std::string, std::string>* getHeadersPtr() {
    return &headers_;
  }

  virtual bool setHeader(const std::string& key, const std::string& value) {
    writeHeaders_[key] = value;
    return true;
  }

  void setHeaders(std::map<std::string, std::string>&& headers) {
    writeHeaders_ = std::move(headers);
  }

  virtual std::vector<uint16_t>& getTransforms() {
    return transforms_;
  }

  virtual uint32_t getMinCompressBytes() {
    return minCompressBytes_;
  }

  PriorityThreadManager::PRIORITY getCallPriority() {
    return callPriority_;
  }

  CLIENT_TYPE getClientType() {
    return ctx_->getHeader()->getClientType();
  }

  std::map<std::string, std::string> releaseHeaders() {
    return ctx_->getHeader()->releaseHeaders();
  }

  virtual const apache::thrift::SaslServer* getSaslServer() const {
    return ctx_->getSaslServer();
  }

  virtual apache::thrift::async::TEventBaseManager* getEventBaseManager() {
    return ctx_->getEventBaseManager();
  }

  virtual void* getUserData() const {
    return ctx_->getUserData();
  }

  virtual void* setUserData(void* data, void (*destructor)(void*) = nullptr) {
    return ctx_->setUserData(data, destructor);
  }

  virtual Cpp2ConnContext* getConnectionContext() const {
    return ctx_;
  }

  bool getStartedProcessing() const {
    return startedProcessing_;
  }

  void setStartedProcessing() {
    startedProcessing_ = true;
  }

 protected:
  // Note:  Header is _not_ thread safe
  virtual apache::thrift::transport::THeader* getHeader() {
    return ctx_->getHeader();
  }

 private:
  Cpp2ConnContext* ctx_;

  // Headers are per-request, not per-connection
  std::map<std::string, std::string> headers_;
  std::map<std::string, std::string> writeHeaders_;
  std::vector<uint16_t> transforms_;
  uint32_t minCompressBytes_;
  PriorityThreadManager::PRIORITY callPriority_;
  bool startedProcessing_ = false;
};

} }

#endif // #ifndef THRIFT_ASYNC_CPP2CONNCONTEXT_H_
