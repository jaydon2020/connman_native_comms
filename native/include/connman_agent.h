#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <sdbus-c++/sdbus-c++.h>

#define CONNMAN_NC_EXPORT __attribute__((visibility("default")))

class CONNMAN_NC_EXPORT ConnmanAgent {
 public:
  ConnmanAgent(sdbus::IConnection& conn, sdbus::ObjectPath object_path);
  ~ConnmanAgent();

  void set_passphrase(const std::string& service_path, const std::string& passphrase);
  void clear_passphrase(const std::string& service_path);

  sdbus::ObjectPath get_path() const { return object_path_; }

 private:
  void Release();
  void ReportError(const sdbus::ObjectPath& path, const std::string& error);
  void RequestBrowser(const sdbus::ObjectPath& path, const std::string& url);
  std::map<std::string, sdbus::Variant> RequestInput(
      const sdbus::ObjectPath& path,
      const std::map<std::string, sdbus::Variant>& fields);
  void Cancel();

  sdbus::ObjectPath object_path_;
  std::unique_ptr<sdbus::IObject> object_;

  std::mutex mutex_;
  std::map<std::string, std::string> passphrases_;
};