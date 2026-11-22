# 双轮式智能导盲杖原型 v0.1.0 · 软件测试记录

测试日期为 2026 年 11 月 22 日。使用 Windows x64、CMake 和 MSVC 构建 Release 版本。测试通过合成栅格与模拟舵机反馈验证 C++ 控制逻辑。

## 自动化测试

CTest 运行 `cane_tests`，结果为 **1/1 通过**。测试覆盖雷达数据解析与无效扫描处理、左右绕行、道路阻塞停步，以及数据超时、反馈异常等情况。

## 五个演示场景

| 场景 | 软件输出 |
| --- | --- |
| 前方通畅（`straight`） | 进入直行状态，舵机目标角度为 0°。 |
| 左侧可绕（`left`） | 依次进入侧向绕行、通过障碍、恢复直行，初始转向命令为 +3.25°。 |
| 右侧可绕（`right`） | 依次进入侧向绕行、通过障碍、恢复直行，初始转向命令为 −3.25°。 |
| 前方阻塞（`blocked`） | 进入停步状态，报告 `obstacle`，舵机控制输出关闭。 |
| 雷达数据超时（`scan-loss`） | 从直行切换为停步，报告 `scan-stale`，舵机控制输出关闭。 |

## 复现方法

在仓库根目录执行：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Windows 使用 Visual Studio 生成器时，运行 `./build/Release/cane_demo.exe left`，把 `left` 替换为表中的场景名称即可。

这份记录的验证范围为电脑端软件。实物避障距离、机械制动效果与现场可靠性需要独立的样机测试记录。
