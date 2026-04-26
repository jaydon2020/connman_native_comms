#include "connman_agent.h"

#include <iostream>

#include "dart_api_dl.h"

static constexpr auto kAgentInterface = "net.connman.Agent";

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

  // Dynamically export the D-Bus Agent interface on our provided connection
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
          .implementedAs([this](const sdbus::ObjectPath& path,
                                const std::map<std::string, sdbus::Variant>&
                                    fields) {
            return this->request_input(path, fields);
          }),
      sdbus::registerMethod("Cancel")
          .implementedAs([this]() { this->cancel(); })
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

void ConnmanAgent::release() {
  // Automatically invoked by ConnMan when unregistering the agent
}

void ConnmanAgent::report_error(const sdbus::ObjectPath& path,
                                const std::string& error) {
  std::cerr << "connman_native_comms: Agent ReportError for " << path
            << ": " << error << "\n";

  // If the password is wrong (e.g. "invalid-key"), ConnMan will immediately
  // ask for it again. We must clear it to avoid an infinite RequestInput loop.
  // The next request will throw a Canceled error, safely aborting the connection.
  clear_passphrase(static_cast<std::string>(path));

  notify_dart(events_port_, "AgentReportError", static_cast<std::string>(path), error);
}

void ConnmanAgent::request_browser(const sdbus::ObjectPath& path,
                                   const std::string& url) {
  std::cerr << "connman_native_comms: Agent RequestBrowser for " << path
            << " (URL: " << url << ")\n";
}

std::map<std::string, sdbus::Variant> ConnmanAgent::request_input(
    const sdbus::ObjectPath& path,
    const std::map<std::string, sdbus::Variant>& fields) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = passphrases_.find(static_cast<std::string>(path));

  if (it == passphrases_.end()) {
    // Notify Dart so the UI can prompt the user for the passphrase.
    notify_dart(events_port_, "AgentRequestInput", static_cast<std::string>(path));

    // Throw a generic D-Bus error that tells ConnMan to cancel the attempt
    throw sdbus::Error(sdbus::Error::Name("net.connman.Agent.Error.Canceled"),
                       "No passphrase provided for service");
  }

  std::map<std::string, sdbus::Variant> response;
  if (fields.find("Passphrase") != fields.end()) {
    response["Passphrase"] = sdbus::Variant(it->second);
  } else if (fields.find("Password") != fields.end()) {
    // Fallback for 802.1x EAP networks which request "Password" instead
    // of "Passphrase"
    response["Password"] = sdbus::Variant(it->second);
  }
  return response;
}

void ConnmanAgent::cancel() {
  // Invoked if the input request is cancelled by ConnMan
}