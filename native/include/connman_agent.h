#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <sdbus-c++/sdbus-c++.h>

#define CONNMAN_NC_EXPORT __attribute__((visibility("default")))

class CONNMAN_NC_EXPORT ConnmanAgent {
 public:
  ConnmanAgent(sdbus::IConnection& conn, sdbus::ObjectPath object_path, int64_t events_port);
  ~ConnmanAgent();

  void set_passphrase(const std::string& service_path, const std::string& passphrase);
  void clear_passphrase(const std::string& service_path);

  sdbus::ObjectPath get_path() const { return object_path_; }

 private:
  void release();
  void report_error(const sdbus::ObjectPath& path, const std::string& error);
  void request_browser(const sdbus::ObjectPath& path, const std::string& url);
  void request_input(
      sdbus::Result<std::map<std::string, sdbus::Variant>>&& result,
      sdbus::ObjectPath path,
      std::map<std::string, sdbus::Variant> fields);
  void cancel();

  sdbus::ObjectPath object_path_;
  std::unique_ptr<sdbus::IObject> object_;
  int64_t events_port_;

  std::mutex mutex_;
  std::map<std::string, std::string> passphrases_;

  struct PendingRequest {
    sdbus::Result<std::map<std::string, sdbus::Variant>> result;
    std::map<std::string, sdbus::Variant> fields;
  };
  std::map<std::string, PendingRequest> pending_requests_;
};