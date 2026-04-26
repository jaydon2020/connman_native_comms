#include "connman_agent.h"
#include <iostream>
#include <unistd.h>
#include "dart_api_dl.h"

static constexpr auto kAgentInterface = "net.connman.Agent";

static void notify_dart(int64_t port, const std::string& event, const std::string& path, const std::string& extra = "") {
  if (port == 0) return;
  Dart_CObject msg_type = {Dart_CObject_kString, {.as_string = (char*)event.c_str()}};
  Dart_CObject msg_path = {Dart_CObject_kString, {.as_string = (char*)path.c_str()}};
  if (!extra.empty()) {
    Dart_CObject msg_extra = {Dart_CObject_kString, {.as_string = (char*)extra.c_str()}};
    Dart_CObject* elements[3] = {&msg_type, &msg_path, &msg_extra};
    Dart_CObject array = {Dart_CObject_kArray, {.as_array = {3, elements}}};
    Dart_PostCObject_DL(port, &array);
  } else {
    Dart_CObject* elements[2] = {&msg_type, &msg_path};
    Dart_CObject array = {Dart_CObject_kArray, {.as_array = {2, elements}}};
    Dart_PostCObject_DL(port, &array);
  }
}

ConnmanAgent::ConnmanAgent(sdbus::IConnection& conn, sdbus::ObjectPath object_path, int64_t events_port)
    : object_path_(std::move(object_path)), events_port_(events_port) {
  std::cerr << "connman_native_comms: [Agent] Exporting at " << object_path_ << "\n";
  object_ = sdbus::createObject(conn, object_path_);

  object_->addVTable(
      sdbus::registerMethod("Release").implementedAs([this]() { this->release(); }),
      sdbus::registerMethod("ReportError").implementedAs([this](const sdbus::ObjectPath& p, const std::string& e) { this->report_error(p, e); }),
      sdbus::registerMethod("RequestBrowser").implementedAs([this](const sdbus::ObjectPath& p, const std::string& u) { this->request_browser(p, u); }),
      sdbus::registerMethod("RequestInput").implementedAs([this](sdbus::Result<std::map<std::string, sdbus::Variant>>&& r, sdbus::ObjectPath p, std::map<std::string, sdbus::Variant> f) {
          this->request_input(std::move(r), std::move(p), std::move(f));
      }),
      sdbus::registerMethod("Cancel").implementedAs([this]() { this->cancel(); })
  ).forInterface(kAgentInterface);
}

ConnmanAgent::~ConnmanAgent() = default;

void ConnmanAgent::set_passphrase(const std::string& service_path, const std::string& passphrase) {
  std::cerr << "connman_native_comms: [Agent] Passphrase received for " << service_path << "\n";
  std::lock_guard<std::mutex> lock(mutex_);
  passphrases_[service_path] = passphrase;

  auto it = pending_requests_.find(service_path);
  if (it != pending_requests_.end()) {
    std::map<std::string, sdbus::Variant> response;
    // Essential: ConnMan expects EXACT keys based on the service type
    if (it->second.fields.count("Passphrase")) response["Passphrase"] = sdbus::Variant(passphrase);
    else if (it->second.fields.count("Password")) response["Password"] = sdbus::Variant(passphrase);
    
    // Always provide Identity if it's in the requested fields
    if (it->second.fields.count("Identity")) response["Identity"] = sdbus::Variant(std::string("anonymous"));

    it->second.result.returnResults(response);
    pending_requests_.erase(it);
    std::cerr << "connman_native_comms: [Agent] RequestInput fulfilled.\n";
  }
}

void ConnmanAgent::clear_passphrase(const std::string& path) {
  std::lock_guard<std::mutex> lock(mutex_);
  passphrases_.erase(path);
  auto it = pending_requests_.find(path);
  if (it != pending_requests_.end()) {
    it->second.result.returnError(sdbus::Error(sdbus::Error::Name("net.connman.Agent.Error.Canceled"), "Canceled by user"));
    pending_requests_.erase(it);
  }
}

void ConnmanAgent::release() { std::cerr << "connman_native_comms: [Agent] Released.\n"; }

void ConnmanAgent::report_error(const sdbus::ObjectPath& path, const std::string& error) {
  std::cerr << "connman_native_comms: [Agent] Error reported for " << path << ": " << error << "\n";
  clear_passphrase(static_cast<std::string>(path));
  notify_dart(events_port_, "AgentReportError", static_cast<std::string>(path), error);
}

void ConnmanAgent::request_browser(const sdbus::ObjectPath& path, const std::string& url) {
  std::cerr << "connman_native_comms: [Agent] RequestBrowser: " << url << "\n";
}

void ConnmanAgent::request_input(sdbus::Result<std::map<std::string, sdbus::Variant>>&& result, sdbus::ObjectPath path, std::map<std::string, sdbus::Variant> fields) {
  std::string path_str = static_cast<std::string>(path);
  std::cerr << "connman_native_comms: [Agent] RequestInput for " << path_str << "\n";
  
  std::lock_guard<std::mutex> lock(mutex_);
  if (passphrases_.count(path_str)) {
    std::map<std::string, sdbus::Variant> response;
    if (fields.count("Passphrase")) response["Passphrase"] = sdbus::Variant(passphrases_[path_str]);
    else if (fields.count("Password")) response["Password"] = sdbus::Variant(passphrases_[path_str]);
    if (fields.count("Identity")) response["Identity"] = sdbus::Variant(std::string("anonymous"));
    result.returnResults(response);
    return;
  }

  notify_dart(events_port_, "AgentRequestInput", path_str);
  pending_requests_[path_str] = {std::move(result), std::move(fields)};
}

void ConnmanAgent::cancel() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto& pair : pending_requests_) {
    pair.second.result.returnError(sdbus::Error(sdbus::Error::Name("net.connman.Agent.Error.Canceled"), "Canceled by ConnMan"));
  }
  pending_requests_.clear();
}
