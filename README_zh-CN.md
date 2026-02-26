<h1 align="center">
    <a href="https://github.com/Illusionna/TCPer" target="_blank"></a>
    <a style="color: #008000;"><b>TCPer</b></a>
</h1>

<h4 align="center"><span>中文</span>&nbsp;&nbsp;&nbsp;&nbsp; | &nbsp;&nbsp;&nbsp;&nbsp;<a href="README.md" target="_blank">English</a></h4>

# 截图

<div align=center>
    <img src="./figures/macOS.png" width="100%" height="100%">
    <img src="./figures/Windows.png" width="90%" height="100%">
</div>

<br>

# 🚀 TCPer 跨操作系统平台传输工具

TCPer 是一款基于 C 语言开发的**轻量级**、**高性能**文件传输工具，它不仅仅是一个简单的 `Socket` 包装器，它更像是一个拥有 TCP 灵魂和火箭速度的数据传送员。

> “像 UDP 一样狂奔，像 TCP 一样靠谱”

<br>

# ✨ 核心特性

- 🛡️ **绝对可靠**：自动进行 **MD5** 完整性校验。如果服务器收到的文件与原始文件不符，它会果断拒绝并清理现场。

- ⏯️ **断点续传**：即使网络闪断，TCPer 也能在重连后从上次中断的位置继续传输，拒绝从零开始。

- ⚡ **线程池驱动**：服务器采用高效率**线程池**设计，轻松处理多个客户端的并发请求。

- 🎨 **终端命令行**：拥有超酷的彩色 ASCII Art 标识，以及实时显示速度、百分比和大小的动态进度条。

- 📂 **智能存储**：所有文件自动归档至 `tcp-storage` 目录文件夹，并具备基本的路径遍历保护。

- 🌐 **跨平台基因**：采用抽象层设计，完美编译且运行于 `Windows`、`macOS` 和 `Linux` 三种操作系统。

<br>

# 🐧 跨平台支持

<div align="center">

| 系统 | 线程实现 | Socket API |
| :-: | :-: | :-: |
| macOS | POSIX Threads| Berkeley Sockets |
| Windows | WIN32 API | Winsock2 |
| Linux | POSIX Threads | Berkeley Sockets |

</div>

<br>

# 🛠️ 快速上手

## 环境

> MinGW GCC：https://github.com/niXman/mingw-builds-binaries

```bash
(macOS) >>> gcc --version

Apple clang version 15.0.0 (clang-1500.3.9.4)
Target: arm64-apple-darwin23.6.0
Thread model: posix
InstalledDir: /Library/Developer/CommandLineTools/usr/bin

(macOS) >>> make --version

GNU Make 3.81
Copyright (C) 2006  Free Software Foundation, Inc.
This is free software; see the source for copying conditions.
There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.
This program built for i386-apple-darwin11.3.0
```

```bash
(Windows) >>> gcc --version

gcc.exe (x86_64-posix-seh-rev0, Built by MinGW-Builds project) 15.2.0
Copyright (C) 2025 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

(Windows) >>> make --version

GNU Make 4.4.1
Built for x86_64-w64-mingw32
Copyright (C) 1988-2023 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
```

```bash
(Linux) >>> gcc --version

gcc (Ubuntu 11.4.0-1ubuntu1~22.04.2) 11.4.0
Copyright (C) 2021 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

(Linux) >>> make --version

GNU Make 4.3
Built for x86_64-pc-linux-gnu
Copyright (C) 1988-2020 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
```

## 下载

```bash
git clone https://github.com/Illusionna/TCPer.git
```

## 编译

```bash
# 使用 4 线程加速编译.
make -j4
```

## 启动

```bash
# 接收端运行程序.
./tcp run [port]
```

## 配置

```bash
# 告诉本地发送端, 接收端服务器在哪里, 格式 tcp config [ipv4:port].
./tcp config 192.168.1.100:31415
```

```bash
# 如果需要查看配置信息.
./tcp config
```

## 发送

```bash
# 发送端传输文件给接收端服务器.
./tcp send ./Desktop/Illusionna/demo.pdf
```

<br>

# 🔍 技术亮点

- 高效 `I/O`：针对大文件优化的 `BUFFER_CAPACITY` 读写，并禁用了 Nagle 算法 `TCP_NODELAY` 以降低延迟。

- 安全握手：内置 `token` 验证机制，如果有人试图用浏览器偷看你的端口，它会优雅地返回一个伪装的 HTML 页面。

- 动态换算：自动将字节转换为 B、KB、MB 或 GB，让进度显示更人性化。

- 异步日志：高性能地记录服务端做出的行为。

<br>

# 开源协议

> [MIT](LICENSE) © Zolio Marling