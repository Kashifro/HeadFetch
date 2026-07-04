# HeadFetch

A mod for [mcpelauncher](https://github.com/minecraft-linux/mcpelauncher-manifest) that adds a player tablist.

## Building

Prerequisites:

- Android NDK r27 or later. [Download](https://developer.android.com/ndk/downloads)
- CMake 3.10 or later

Replace `/path/to/ndk` with your Android NDK path:

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/ndk/build/cmake/android.toolchain.cmake \
  -DANDROID_PLATFORM=21 \
  -DANDROID_ABI=<abi> \
  -DANDROID_STL=c++_shared \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build --target headfetch
```

Supported ABIs:

- `x86_64`
- `arm64-v8a`
