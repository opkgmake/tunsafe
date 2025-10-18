// SPDX-License-Identifier: AGPL-1.0-only
// Copyright (C) 2024
#include "socks5_tunnel_runner.h"

#include "util.h"

#include <chrono>
#include <sstream>
#include <string.h>
#include <unistd.h>

#include "third_party/hev-socks5-tunnel/include/hev-socks5-tunnel.h"

namespace {
std::string EscapeYaml(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  for (char ch : value) {
    switch (ch) {
      case '\\':
      case '"':
        out.push_back('\\');
        out.push_back(ch);
        break;
      case '\n':
        out.append("\\n");
        break;
      case '\r':
        out.append("\\r");
        break;
      case '\t':
        out.append("\\t");
        break;
      default:
        out.push_back(ch);
        break;
    }
  }
  out.push_back('"');
  return out;
}

std::string Defaulted(const std::string &value, const std::string &fallback) {
  return value.empty() ? fallback : value;
}
}  // namespace

Socks5TunnelRunner::Socks5TunnelRunner() : running_(false), exit_code_(0) {}

Socks5TunnelRunner::~Socks5TunnelRunner() {
  Stop();
}

std::string Socks5TunnelRunner::BuildConfig(const TunInterface::TunConfig::Socks5Settings &settings,
                                            int mtu) const {
  std::ostringstream ss;
  ss << "tunnel:\n";
  ss << "  name: tunsafe-socks\n";
  ss << "  mtu: " << (mtu > 0 ? mtu : 1500) << "\n";
  ss << "  multi-queue: false\n";
  std::string ipv4 = Defaulted(settings.tunnel_ipv4, "198.18.0.1");
  ss << "  ipv4: " << EscapeYaml(ipv4) << "\n";
  if (!settings.tunnel_ipv6.empty())
    ss << "  ipv6: " << EscapeYaml(settings.tunnel_ipv6) << "\n";

  ss << "socks5:\n";
  ss << "  address: " << EscapeYaml(settings.server_address) << "\n";
  ss << "  port: " << settings.server_port << "\n";
  ss << "  udp: " << EscapeYaml(Defaulted(settings.udp_mode, "udp")) << "\n";
  if (settings.pipeline)
    ss << "  pipeline: true\n";
  if (!settings.username.empty())
    ss << "  username: " << EscapeYaml(settings.username) << "\n";
  if (!settings.password.empty())
    ss << "  password: " << EscapeYaml(settings.password) << "\n";

  ss << "misc:\n";
  ss << "  log-level: " << EscapeYaml(Defaulted(settings.log_level, "none")) << "\n";
  ss << "  task-stack-size: 98304\n";
  return ss.str();
}

bool Socks5TunnelRunner::Start(const TunInterface::TunConfig::Socks5Settings &settings, int fd, int mtu) {
  Stop();
  if (settings.server_address.empty() || settings.server_port == 0) {
    last_error_ = "未提供有效的 Socks5Proxy";
    return false;
  }
  std::string config = BuildConfig(settings, mtu);
  int dup_fd = dup(fd);
  if (dup_fd < 0) {
    last_error_ = "无法复制隧道文件描述符";
    return false;
  }

  running_ = true;
  exit_code_ = 0;
  last_error_.clear();
  thread_ = std::thread(&Socks5TunnelRunner::ThreadMain, this, dup_fd, std::move(config));

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  std::unique_lock<std::mutex> lock(mutex_);
  if (!running_) {
    int code = exit_code_;
    lock.unlock();
    if (thread_.joinable())
      thread_.join();
    lock.lock();
    running_ = false;
    lock.unlock();
    if (code < 0)
      last_error_ = "Socks5 隧道初始化失败 (返回值 " + std::to_string(code) + ")";
    else
      last_error_ = "Socks5 隧道已退出 (返回值 " + std::to_string(code) + ")";
    return false;
  }
  return true;
}

void Socks5TunnelRunner::Stop() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!running_) {
    lock.unlock();
    if (thread_.joinable())
      thread_.join();
    return;
  }
  hev_socks5_tunnel_quit();
  lock.unlock();
  if (thread_.joinable())
    thread_.join();
  lock.lock();
  running_ = false;
}

void Socks5TunnelRunner::ThreadMain(int fd, std::string config) {
  int result = hev_socks5_tunnel_main_from_str(
      reinterpret_cast<const unsigned char *>(config.data()),
      static_cast<unsigned int>(config.size()), fd);
  close(fd);
  std::lock_guard<std::mutex> lock(mutex_);
  exit_code_ = result;
  running_ = false;
}
