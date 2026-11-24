# 双轮式智能导盲杖 Linux 演示

提供 x86_64 图形界面和命令行程序，使用与原型相同的 C++ 控制逻辑。可播放、逐帧查看和重置五个离线仿真场景。路线图用于展示状态变化，程序的输出使用模拟输入。

## 下载和运行

- **Debian 12 / Ubuntu 24.04** 可使用 `sudo apt install ./smart-guide-cane_0.1.0_amd64.deb` 安装。安装后在应用菜单打开「双轮式智能导盲杖」或运行 `smart-guide-cane`。
- 使用 **tar.gz 解压包**时，在解压后的目录运行 `./bin/smart-guide-cane`。需要 GTK 3 桌面运行库和可显示中文的字体。
- 命令行程序为 `smart-guide-cane-cli`。可传入 `straight`、`left`、`right`、`blocked` 或 `scan-loss`。使用解压包时，从包内的 `bin/` 启动即可。

## 从源码构建

需要 CMake、C++17 编译器、pkg-config 和 GTK 3 开发库。Debian 或 Ubuntu 可安装 `build-essential cmake pkg-config libgtk-3-dev`。安装 `xvfb xauth` 后，CTest 会在虚拟显示器中验证界面。

```sh
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release -DSMART_GUIDE_CANE_LINUX_GUI=ON
cmake --build build-linux --parallel
ctest --test-dir build-linux --output-on-failure
./build-linux/smart-guide-cane
```
