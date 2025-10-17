// SPDX-License-Identifier: AGPL-1.0-only
// Copyright (C) 2024
#pragma once

#include "netapi.h"

#include <mutex>
#include <string>
#include <thread>

class Socks5TunnelRunner {
public:
  Socks5TunnelRunner();
  ~Socks5TunnelRunner();

  bool Start(const TunInterface::TunConfig::Socks5Settings &settings, int fd, int mtu);
  void Stop();

  bool running() const { return running_; }
  const std::string &last_error() const { return last_error_; }

private:
  std::string BuildConfig(const TunInterface::TunConfig::Socks5Settings &settings, int mtu) const;
  void ThreadMain(int fd, std::string config);

  std::thread thread_;
  mutable std::mutex mutex_;
  bool running_;
  int exit_code_;
  std::string last_error_;
};
