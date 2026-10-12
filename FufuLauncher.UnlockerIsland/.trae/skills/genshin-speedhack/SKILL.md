---
name: genshin-speedhack
description: 保存 FufuLauncher 原神 DLL 注入项目中 SpeedHack 变速功能的完整实现机制与坑。当用户要求修改 SpeedHack 模块、调试时间变速异常、解释 QPC 时间劫持原理或将其移植到新游戏版本时使用。
---

# SpeedHack 变速实现（Genshin IL2CPP DLL 注入）

核心代码在 `SpeedHack/SpeedHack.cpp`，函数指针在 `Core/SharedState.h/.cpp`，配置项 `enable_speedhack` 与 `game_speed` 在 `Config/Config.h/.cpp`。

## 原理：输出参数劫持（不是返回值劫持）

`QueryPerformanceCounter` 的签名决定了一切：

```
BOOL QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
```

- **返回值（BOOL）只是成功标志**，真正的 64 位时间戳写在**输出参数指针指向的内存**里。
- 因此变速的方式是：先通过 trampoline 调原函数拿真实时间（写到自己的临时变量），再把**缩放后的假时间写回调用方传入的 `lpPerformanceCount->QuadPart`**，最后返回 TRUE。
- 劫持的是"每次调用之间的增量"而非绝对值：`假时间 += (真实时间 - 上次真实时间) × game_speed`，绝对值跳变会被游戏检测/运算炸掉。

## 增量缩放的关键细节（照抄这几个做法）

1. **小数余量累积器** `s_FractionalRemainder`：`Δ真实 × game_speed` 通常是分数个 QPC tick，直接取整会丢时间导致变速不准。正确做法是保留 exactDelta 的小数部分，累积到下次调用（`SpeedHack.cpp:56-59`）。
2. **重入保护** `static thread_local bool g_InHook`：hook 内部调原 QPC 拿真实时间时会再次进入 hook。thread_local 只护本线程，重入时直接走 trampoline（`SpeedHack.cpp:24,32-36`）。
3. **预解析原函数地址**：kernel32 的 QPC 只是到 kernelbase.dll 的转发存根，必须 `GetProcAddress(kernelbase, "QueryPerformanceCounter")`，kernelbase 拿不到再退 kernel32（`SpeedHack.cpp:124-131`）。
4. **状态初始化**：首次调用时 `s_LastReal = s_LastFake = 当前真实时间`，假时间从真实时间起算，避免从 0 跳变。
5. **开关关闭时也要走假时间累计路径**：`enable_speedhack=false` 时增量乘 1 照常累计，保证开关切换瞬间时间连续不回跳（`SpeedHack.cpp:61-63`）。

## Hook 安装方式

MinHook 流程：`GetProcAddress` 拿地址 → `MH_CreateHook(addr, &hk_QPC, (LPVOID*)&o_QueryPerformanceCounter)` → 交给统一的 `MH_EnableHook(MH_ALL_HOOKS)` 激活（在 Hooks.cpp 里统一启用，不要在模块内单独 Enable）。

## 已知限制与坑（都是实测结论）

- **game_speed != 1 时游戏内部帧率限制器失效**：游戏用假 QPC 计算与目标帧的 sleep 时长，变速后判断逻辑错乱，帧率可能跑到 2000+。这是假时间与游戏限帧逻辑交互的固有现象，不是 bug 修复目标，除非同时处理限帧路径。
- **Sleep 与假时间不同步**：系统 Sleep 用真实时间。变速 >1 时 CPU 算得快、睡得照旧，速度打折扣；变速 <1 时游戏"时间预算"远大于真实时间，Sleep 变得极短，帧率暴涨。
- **GetTickCount64 hook 实际是死代码**：`hk_GetTickCount64` 第 73 行开头就 `return o_GetTickCount64()` 直接转发，后面整个假时间累计块不可达。历史原因保留。若要启用 GTC 变速，删掉该行并注意 ULONGLONG 只有 100ns×100 的 `s_LastFakeTick100` 精度技巧。
- **绝对不要改返回值来变速**：QPC 返回值是 BOOL，把假时间放在返回值里只会把布尔值搞坏，时间戳必须写输出参数。
- **rdtsc 是绕不过的旁路**：任何用 `__rdtsc` 计时的代码（如本项目 RenderSkip 的帧判定）不受 QPC hook 影响。反过来，想在变速时保持自研计时准确，用 rdtsc 而不是 QPC。

## 移植到新游戏版本的检查单

1. QPC/GTC 是 Windows API，地址不随游戏版本变——无需改偏移。
2. 检查 `Config` 里 `enable_speedhack` / `game_speed` 字段名与配置文件解析是否还在。
3. 确认 `SharedState` 中 `o_QueryPerformanceCounter` / `o_GetTickCount64` 指针与 typedef `tQueryPerformanceCounter` / `tGetTickCount64` 完整。
4. 若游戏更新后变速无效，先确认 hook 安装日志（`[SpeedHack] Hook Ready.`），再用 rdtsc 对比 QPC 输出验证缩放是否生效。
