#include "connman_agent.h"

#include <iostream>

static constexpr auto kAgentInterface = "net.connman.Agent";

ConnmanAgent::ConnmanAgent(sdbus::IConnection& conn, sdbus::ObjectPath object_path)
    : object_path_(std::move(object_path)) {
  object_ = sdbus::createObject(conn, object_path_);

  // Dynamically export the D-Bus Agent interface on our provided connection
  object_->addVTable(
      sdbus::registerMethod("Release")
          .implementedAs([this]() { this->Release(); }),
      sdbus::registerMethod("ReportError")
          .implementedAs([this](const sdbus::ObjectPath& path, const std::string& error) {
            this->ReportError(path, error);
          }),
      sdbus::registerMethod("RequestBrowser")
          .implementedAs([this](const sdbus::ObjectPath& path, const std::string& url) {
            this->RequestBrowser(path, url);
          }),
      sdbus::registerMethod("RequestInput")
          .implementedAs([this](const sdbus::ObjectPath& path,
                                const std::map<std::string, sdbus::Variant>& fields) {
            return this->RequestInput(path, fields);
          }),
      sdbus::registerMethod("Cancel")
          .implementedAs([this]() { this->Cancel(); })
  ).forInterface(kAgentInterface);
}

ConnmanAgent::~ConnmanAgent() = default;

void ConnmanAgent::set_passphrase(const std::string& service_path,
                                  const std::string& passphrase) {
  std::lock_guard<std::mutex> lock(mutex_);
  passphrases_[service_path] = passphrase;
}

void ConnmanAgent::clear_passphrase(const std::string& service_path) {
  std::lock_guard<std::mutex> lock(mutex_);
  passphrases_.erase(service_path);
}

void ConnmanAgent::Release() {
  // Automatically invoked by ConnMan when unregistering the agent
}

void ConnmanAgent::ReportError(const sdbus::ObjectPath& path, const std::string& error) {
  std::cerr << "connman_native_comms: Agent ReportError for " << path
            << ": " << error << "\n";
}

void ConnmanAgent::RequestBrowser(const sdbus::ObjectPath& path, const std::string& url) {
  std::cerr << "connman_native_comms: Agent RequestBrowser for " << path
            << " (URL: " << url << ")\n";
}

std::map<std::string, sdbus::Variant> ConnmanAgent::RequestInput(
    const sdbus::ObjectPath& path,
    const std::map<std::string, sdbus::Variant>& fields) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = passphrases_.find(path);

  if (it == passphrases_.end()) {
    // Throw a generic D-Bus error that tells ConnMan to cancel the attempt
    throw sdbus::Error(sdbus::Error::Name("net.connman.Agent.Error.Canceled"), 
                       "No passphrase provided for service");
  }

  std::map<std::string, sdbus::Variant> response;
  if (fields.find("Passphrase") != fields.end()) {
    response["Passphrase"] = sdbus::Variant(it->second);
  }
  return response;
}

void ConnmanAgent::Cancel() {
  // Invoked if the input request is cancelled by ConnMan
}