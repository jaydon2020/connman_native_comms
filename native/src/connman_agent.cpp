#include "connman_agent.h"

#include <iostream>

#include "dart_api_dl.h"

static constexpr auto kAgentInterface = "net.connman.Agent";
static constexpr auto kLegacyAgentInterface = "org.moblin.connman.Agent";

// Helper to send Dart array messages, e.g. ["AgentReportError", "/path", "error"]
static void notify_dart(int64_t port, const std::string& event,
                        const std::string& path, const std::string& extra = "") {
  if (port == 0) return;

  Dart_CObject msg_type;
  msg_type.type = Dart_CObject_kString;
  msg_type.value.as_string = event.c_str();

  Dart_CObject msg_path;
  msg_path.type = Dart_CObject_kString;
  msg_path.value.as_string = path.c_str();

  if (!extra.empty()) {
    Dart_CObject msg_extra;
    msg_extra.type = Dart_CObject_kString;
    msg_extra.value.as_string = extra.c_str();

    Dart_CObject* elements[3] = {&msg_type, &msg_path, &msg_extra};

    Dart_CObject array;
    array.type = Dart_CObject_kArray;
    array.value.as_array.length = 3;
    array.value.as_array.values = elements;

    Dart_PostCObject_DL(port, &array);
  } else {
    Dart_CObject* elements[2] = {&msg_type, &msg_path};

    Dart_CObject array;
    array.type = Dart_CObject_kArray;
    array.value.as_array.length = 2;
    array.value.as_array.values = elements;

    Dart_PostCObject_DL(port, &array);
  }
}

ConnmanAgent::ConnmanAgent(sdbus::IConnection& conn,
                           sdbus::ObjectPath object_path,
                           int64_t events_port)
    : object_path_(std::move(object_path)), events_port_(events_port) {
  object_ = sdbus::createObject(conn, object_path_);

  // Register modern interface
  object_->addVTable(
      sdbus::registerMethod("Release")
          .implementedAs([this]() { this->release(); }),
      sdbus::registerMethod("ReportError")
          .implementedAs([this](const sdbus::ObjectPath& path,
                                const std::string& error) {
            this->report_error(path, error);
          }),
      sdbus::registerMethod("RequestBrowser")
          .implementedAs([this](const sdbus::ObjectPath& path,
                                const std::string& url) {
            this->request_browser(path, url);
          }),
      sdbus::registerMethod("RequestInput")
          .implementedAs([this](sdbus::Result<std::map<std::string, sdbus::Variant>>&& result,
                                sdbus::ObjectPath path,
                                std::map<std::string, sdbus::Variant> fields) {
            this->request_input(std::move(result), std::move(path), std::move(fields));
          }),
      sdbus::registerMethod("Cancel")
          .implementedAs([this]() { this->cancel(); })
  ).forInterface(kAgentInterface);

  // Also register legacy interface for maximum compatibility
  object_->addVTable(
      sdbus::registerMethod("Release")
          .implementedAs([this]() { this->release(); }),
      sdbus::registerMethod("ReportError")
          .implementedAs([this](const sdbus::ObjectPath& path,
                                const std::string& error) {
            this->report_error(path, error);
          }),
      sdbus::registerMethod("RequestInput")
          .implementedAs([this](sdbus::Result<std::map<std::string, sdbus::Variant>>&& result,
                                sdbus::ObjectPath path,
                                std::map<std::string, sdbus::Variant> fields) {
            this->request_input(std::move(result), std::move(path), std::move(fields));
          }),
      sdbus::registerMethod("Cancel")
          .implementedAs([this]() { this->cancel(); })
  ).forInterface(kLegacyAgentInterface);
}

ConnmanAgent::~ConnmanAgent() = default;

void ConnmanAgent::set_passphrase(const std::string& service_path,
                                  const std::string& passphrase) {
  std::cerr << "connman_native_comms: [Agent] Setting passphrase for " << service_path << "\n";
  std::lock_guard<std::mutex> lock(mutex_);
  passphrases_[service_path] = passphrase;

  // Fulfill any pending async D-Bus request for this service
  auto it = pending_requests_.find(service_path);
  if (it != pending_requests_.end()) {
    std::map<std::string, sdbus::Variant> response;
    if (it->second.fields.find("Passphrase") != it->second.fields.end()) {
      response["Passphrase"] = sdbus::Variant(passphrase);
    } else if (it->second.fields.find("Password") != it->second.fields.end()) {
      response["Password"] = sdbus::Variant(passphrase);
    }

    if (it->second.fields.find("Identity") != it->second.fields.end()) {
      response["Identity"] = sdbus::Variant(std::string("anonymous"));
    }

    std::cerr << "connman_native_comms: [Agent] Fulfilling pending RequestInput for " << service_path << "\n";
    it->second.result.returnResults(response);
    pending_requests_.erase(it);
  }
}

void ConnmanAgent::clear_passphrase(const std::string& service_path) {
  std::cerr << "connman_native_comms: [Agent] Clearing passphrase/canceling for " << service_path << "\n";
  std::lock_guard<std::mutex> lock(mutex_);
  passphrases_.erase(service_path);

  // Fulfill with error if the user explicitly canceled the password prompt
  auto it = pending_requests_.find(service_path);
  if (it != pending_requests_.end()) {
    it->second.result.returnError(sdbus::Error(
        sdbus::Error::Name("net.connman.Agent.Error.Canceled"),
        "User canceled input"));
    pending_requests_.erase(it);
  }
}

void ConnmanAgent::release() {
  std::cerr << "connman_native_comms: [Agent] Released by ConnMan\n";
}

void ConnmanAgent::report_error(const sdbus::ObjectPath& path,
                                const std::string& error) {
  std::cerr << "connman_native_comms: [Agent] ReportError for " << path
            << ": " << error << "\n";

  // If the password is wrong (e.g. "invalid-key"), ConnMan will immediately
  // ask for it again. We must clear it to avoid an infinite RequestInput loop.
  clear_passphrase(static_cast<std::string>(path));

  notify_dart(events_port_, "AgentReportError", static_cast<std::string>(path), error);
}

void ConnmanAgent::request_browser(const sdbus::ObjectPath& path,
                                   const std::string& url) {
  std::cerr << "connman_native_comms: [Agent] RequestBrowser for " << path
            << " (URL: " << url << ")\n";
}

void ConnmanAgent::request_input(
    sdbus::Result<std::map<std::string, sdbus::Variant>>&& result,
    sdbus::ObjectPath path,
    std::map<std::string, sdbus::Variant> fields) {
  std::string path_str = static_cast<std::string>(path);
  std::cerr << "connman_native_comms: [Agent] RequestInput called for " << path_str << "\n";
  for (const auto& [key, _] : fields) {
      std::cerr << "  Requested field: " << key << "\n";
  }

  std::lock_guard<std::mutex> lock(mutex_);
  auto it = passphrases_.find(path_str);

  if (it != passphrases_.end()) {
    std::map<std::string, sdbus::Variant> response;
    if (fields.find("Passphrase") != fields.end()) {
      response["Passphrase"] = sdbus::Variant(it->second);
    } else if (fields.find("Password") != fields.end()) {
      response["Password"] = sdbus::Variant(it->second);
    }

    if (fields.find("Identity") != fields.end()) {
      response["Identity"] = sdbus::Variant(std::string("anonymous"));
    }

    std::cerr << "connman_native_comms: [Agent] Returning cached passphrase for " << path_str << "\n";
    result.returnResults(response);
    return;
  }

  // Notify Dart so the UI can prompt the user for the passphrase.
  std::cerr << "connman_native_comms: [Agent] Notifying Dart about RequestInput for " << path_str << "\n";
  notify_dart(events_port_, "AgentRequestInput", path_str);

  // Store the pending result to be fulfilled when set_passphrase is called
  pending_requests_.erase(path_str);
  pending_requests_.emplace(path_str,
                            PendingRequest{std::move(result), std::move(fields)});
}

void ConnmanAgent::cancel() {
  std::cerr << "connman_native_comms: [Agent] Cancel called by ConnMan\n";
  std::lock_guard<std::mutex> lock(mutex_);
  // Invoked if the input request is cancelled by ConnMan
  for (auto& pair : pending_requests_) {
    pair.second.result.returnError(sdbus::Error(
        sdbus::Error::Name("net.connman.Agent.Error.Canceled"),
        "Canceled by ConnMan"));
  }
  pending_requests_.clear();
}
