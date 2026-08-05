# Win16 移植库集合

本仓库包含多个开源项目的 Win16 移植版本，每个子目录对应一个项目，

并保留原项目的许可证。除了WolfSSL，其它都是我移植的

本项目适用于 Windows for Workgroups 3.11 上的复古浏览器开发。

| 目录 | 项目 | 原始许可证 |
|------|------|-----------|
| `quickjs/` | QuickJS | MIT |
| `libcss/` | LibCSS + 依赖 | MIT |
| `gumbo/` | Gumbo-parser | Apache-2.0 |
| `freetype/` | FreeType | FTL (BSD-like) 或 GPLv2 |
| `wolfssl/` | WolfSSL | GPLv3 |

## 支持

除WolfSSL外的其他库版本是现在(2026/8/5)最新的

仅保证基础功能正常，如果你发现有问题可以把源代码下载下来进行更新，欢迎修补

wolfssl源代码在[这里](https://dialup.net/wingpt/download/wolfssl10.zip)下载

## 许可证

- 每个子目录下的代码受其原项目许可证约束，详见各子目录中的 `LICENSE` 文件。
- 本仓库中由 你的名字 编写的移植代码（如 `win16_compat.h`、构建脚本、测试程序等）采用 MIT 许可证。
