# Win16 Porting Library Collection

This repository contains Win16 ports of several open-source projects.
Each subdirectory corresponds to one project.

These libraries are intended for retro browser development
on Windows for Workgroups 3.11.

| Directory | Project | Original License |
|-----------|---------|------------------|
| `quickjs/` | QuickJS | MIT |
| `libcss/` | LibCSS + dependencies | MIT |
| `gumbo/` | Gumbo-parser | Apache-2.0 |
| `freetype/` | FreeType | FTL (BSD-like) or GPLv2 |
| `wolfssl/` | WolfSSL | GPLv3 |

## Support

All libraries except WolfSSL are the latest versions as of 2026-08-05.

Only basic functionality is guaranteed to work.
If you find any issues, feel free to download the source code
and submit fixes — contributions are welcome.

Due to WolfSSL's license restrictions, the Win16 port of its
source code is provided separately:

[Download wolfssl-win16 source](https://dialup.net/wingpt/download/wolfssl10.zip)

## License

- The code in each subdirectory is governed by the original project's
  license. See the `LICENSE` file in each subdirectory for details.
- The porting code written by qv1145145 (such as `win16_compat.h`,
  build scripts, and test programs) is released under the MIT License.
