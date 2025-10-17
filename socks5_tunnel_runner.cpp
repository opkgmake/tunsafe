// SPDX-License-Identifier: AGPL-1.0-only
// Copyright (C) 2024
#include "socks5_tunnel_runner.h"

#include "util.h"

#include <chrono>
#include <dlfcn.h>
#include <limits.h>
#include <sstream>
#include <string.h>
#include <unistd.h>
#include <vector>

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

Socks5TunnelRunner::Socks5TunnelRunner()
    : lib_handle_(nullptr),
      main_fn_(nullptr),
      quit_fn_(nullptr),
      running_(false),
      exit_code_(0) {}

Socks5TunnelRunner::~Socks5TunnelRunner() {
  Stop();
}

bool Socks5TunnelRunner::LoadLibrary(const std::string &path) {
  std::vector<std::string> candidates;
  if (!path.empty()) {
    candidates.push_back(path);
  } else {
    candidates.push_back("libhev-socks5-tunnel.so");
    candidates.push_back("./libhev-socks5-tunnel.so");
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len > 0) {
      exe_path[len] = '\0';
      std::string exe_dir(exe_path);
      size_t slash = exe_dir.find_last_of('/');
      if (slash != std::string::npos)
        exe_dir.resize(slash + 1);
      else
        exe_dir.clear();
      candidates.push_back(exe_dir + "libhev-socks5-tunnel.so");
    }
  }

  const char *last_error = nullptr;
  std::vector<std::string> error_details;
  for (const std::string &resolved : candidates) {
    lib_handle_ = dlopen(resolved.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (lib_handle_)
      break;
    last_error = dlerror();
    if (last_error) {
      std::string message(last_error);
      if (message.find("Dynamic loading not supported") != std::string::npos) {
        last_error_ =
            "当前的 TunSafe 构建不支持动态加载 (例如使用 ENABLE_STATIC=1 编译)。\n"
            "Socks5 模式需要启用动态链接，请使用支持 dlopen 的构建方式重新编译";
        return false;
      }
      error_details.emplace_back(resolved + ": " + message);
    } else {
      error_details.emplace_back(resolved + ": 未知错误");
    }
  }
  if (!lib_handle_) {
    last_error_ = "无法加载 libhev-socks5-tunnel";
    if (!error_details.empty()) {
      last_error_ += "。尝试的路径: ";
      for (size_t i = 0; i < error_details.size(); ++i) {
        if (i)
          last_error_ += "; ";
        last_error_ += error_details[i];
      }
      last_error_ += "。请确认已按照 hev-socks5-tunnel 项目的 make shared 生成共享库";
    } else if (last_error) {
      last_error_ += std::string(": ") + last_error;
    }
    return false;
  }
  main_fn_ = reinterpret_cast<MainFromStrFn>(dlsym(lib_handle_, "hev_socks5_tunnel_main_from_str"));
  quit_fn_ = reinterpret_cast<QuitFn>(dlsym(lib_handle_, "hev_socks5_tunnel_quit"));
  if (!main_fn_ || !quit_fn_) {
    last_error_ = "libhev-socks5-tunnel 缺少必要的导出函数";
    dlclose(lib_handle_);
    lib_handle_ = nullptr;
    main_fn_ = nullptr;
    quit_fn_ = nullptr;
    return false;
  }
  return true;
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
  ss << "  log-level: " << EscapeYaml(Defaulted(settings.log_level, "warn")) << "\n";
  ss << "  task-stack-size: 98304\n";
  return ss.str();
}

bool Socks5TunnelRunner::Start(const TunInterface::TunConfig::Socks5Settings &settings, int fd, int mtu) {
  Stop();
  if (settings.server_address.empty() || settings.server_port == 0) {
    last_error_ = "未提供有效的 Socks5Proxy";
    return false;
  }
  if (!LoadLibrary(settings.library_path))
    return false;

  std::string config = BuildConfig(settings, mtu);
  int dup_fd = dup(fd);
  if (dup_fd < 0) {
    last_error_ = "无法复制隧道文件描述符";
    dlclose(lib_handle_);
    lib_handle_ = nullptr;
    main_fn_ = nullptr;
    quit_fn_ = nullptr;
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
    if (lib_handle_) {
      dlclose(lib_handle_);
      lib_handle_ = nullptr;
    }
    main_fn_ = nullptr;
    quit_fn_ = nullptr;
    running_ = false;
    if (code < 0)
      last_error_ = "hev_socks5_tunnel_main_from_str 启动失败";
    else
      last_error_ = "hev_socks5_tunnel 提前退出";
    return false;
  }
  return true;
}

void Socks5TunnelRunner::Stop() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!lib_handle_) {
    running_ = false;
    lock.unlock();
    if (thread_.joinable())
      thread_.join();
    return;
  }
  if (running_ && quit_fn_)
    quit_fn_();
  lock.unlock();
  if (thread_.joinable())
    thread_.join();
  lock.lock();
  running_ = false;
  if (lib_handle_) {
    dlclose(lib_handle_);
    lib_handle_ = nullptr;
  }
  main_fn_ = nullptr;
  quit_fn_ = nullptr;
}

void Socks5TunnelRunner::ThreadMain(int fd, std::string config) {
  int result = main_fn_(reinterpret_cast<const unsigned char *>(config.data()),
                        static_cast<unsigned int>(config.size()), fd);
  close(fd);
  std::lock_guard<std::mutex> lock(mutex_);
  exit_code_ = result;
  running_ = false;
}
