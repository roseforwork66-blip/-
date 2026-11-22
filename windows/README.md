# 双轮式智能导盲杖 Windows 演示

适用于 Windows 10 和 Windows 11 的 64 位（x64）电脑。双击 `SmartGuideCane.exe` 即可打开，便携包包含程序、运行说明和许可证。

选择前方通畅、左侧可绕、右侧可绕、前方阻塞或雷达超时。点击「播放」自动回放，也可以点击「下一帧」查看每次状态变化，或点击「重置」回到起始画面。路线图为示意，右侧数据来自与命令行演示共用的 C++ 控制程序。

本应用使用离线仿真输入。实体导盲杖的连接、控制与实物测试需要独立验证。

## 从源码构建

在 Windows 上使用 Visual Studio C++ 工具和 CMake，从仓库根目录执行：

```powershell
cmake -S . -B build -A x64 -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

生成的应用位于 `build/Release/SmartGuideCane.exe`。CTest 会校验五个场景的输出、播放和逐帧操作，以及停步时的舵机状态。
