# tunsafe
<p align="center">
  <img alt="GitHub Created At" src="https://img.shields.io/github/created-at/lmq8267/tunsafe?logo=github&label=%E5%88%9B%E5%BB%BA%E6%97%A5%E6%9C%9F">
<a href="https://hits.seeyoufarm.com"><img src="https://hits.seeyoufarm.com/api/count/incr/badge.svg?url=https%3A%2F%2Fgithub.com%2Flmq8267%2Ftunsafe&count_bg=%2395C10D&title_bg=%23555555&icon=github.svg&icon_color=%238DC409&title=%E8%AE%BF%E9%97%AE%E6%95%B0&edge_flat=false"/></a>
<a href="https://github.com/lmq8267/tunsafe/releases"><img src="https://img.shields.io/github/downloads/lmq8267/tunsafe/total?logo=github&label=%E4%B8%8B%E8%BD%BD%E9%87%8F"></a>
<a href="https://github.com/lmq8267/tunsafe/graphs/contributors"><img src="https://img.shields.io/github/contributors-anon/lmq8267/tunsafe?logo=github&label=%E8%B4%A1%E7%8C%AE%E8%80%85"></a>
<a href="https://github.com/lmq8267/tunsafe/releases/"><img src="https://img.shields.io/github/release/lmq8267/tunsafe?logo=github&label=%E6%9C%80%E6%96%B0%E7%89%88%E6%9C%AC"></a>
<a href="https://github.com/lmq8267/tunsafe/issues"><img src="https://img.shields.io/github/issues-raw/lmq8267/tunsafe?logo=github&label=%E9%97%AE%E9%A2%98"></a>
<a href="https://github.com/lmq8267/tunsafe/discussions"><img src="https://img.shields.io/github/discussions/lmq8267/tunsafe?logo=github&label=%E8%AE%A8%E8%AE%BA"></a>
<a href="GitHub repo size"><img src="https://img.shields.io/github/repo-size/lmq8267/tunsafe?logo=github&label=%E4%BB%93%E5%BA%93%E5%A4%A7%E5%B0%8F"></a>
</p>
  
项目地址：https://github.com/TunSafe/TunSafe

liaohcai大佬的相关教程:
https://www.right.com.cn/forum/thread-8348737-1-1.html

## Socks5 后端支持

在部分缺少 TUN 设备的 Linux 设备上，可以通过启用内置的 Socks5 隧道后端来运行 TunSafe。该模式依赖
[hev-socks5-server](https://github.com/heiher/hev-socks5-server) 提供出口，TunSafe 会将 TUN 读写转换为
用户态 Socks5 流量，因此无需再创建 `/dev/net/tun` 设备。

在配置文件的 `[Interface]` 段落中添加以下字段即可启用：

```
Socks5Proxy = 127.0.0.1:1080
# 可选: Socks5Username = user
# 可选: Socks5Password = pass
# 可选: Socks5UdpMode = tcp   # 默认 udp
# 可选: Socks5Pipeline = true
# 可选: Socks5TunnelIPv4 = 198.18.0.1
# 可选: Socks5TunnelIPv6 = fc00::1
```

TunSafe 已内置 [hev-socks5-tunnel](https://github.com/heiher/hev-socks5-tunnel) 所需的客户端实现，
无需再额外放置 `libhev-socks5-tunnel.so`。只要系统中存在 Socks5 服务端并在配置中填写正确的地址，即可直接启用。

