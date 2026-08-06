# Win16 Porting Library Collection

This repository contains Win16 ports of several open source projects,
each in its own subdirectory.

These libraries are intended for retro browser development
on Windows for Workgroups 3.11.

Note: the WolfSSL port was done by DialupDotNet.

| Directory | Project | Original License |
|-----------|---------|------------------|
| `quickjs/` | QuickJS | MIT |
| `libcss/` | LibCSS + dependencies | MIT |
| `gumbo/` | Gumbo-parser | Apache-2.0 |
| `freetype/` | FreeType | FTL (BSD-like) or GPLv2 |
| `wolfssl/` | WolfSSL | GPLv3 |

## Support

All libraries except WolfSSL are up to date as of 2025-08-05.

Only basic functionality is guaranteed to work.
If you find issues, feel free to download the source code and fix them.
Contributions are welcome.

Due to WolfSSL's license terms, the Win16 port source is provided separately.
You can download it [here](https://dialup.net/wingpt/download/wolfssl10.zip)
and compile it manually.

## License

- The code in each subdirectory is governed by the original project's license.
  See the `LICENSE` file in each subdirectory for details.
- The porting code written by qv1145145 (such as `win16_compat.h`,
  build scripts, and test programs) is released under the MIT License.
