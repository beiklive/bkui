# BeikUI

BeikUI 是一个面向跨平台场景的 C++20 UI / 渲染基础工程，当前重点面向 Windows 与 Nintendo Switch，目标是把平台层、资源层、RHI 层和图形后端拆开，方便后续继续扩展控件系统、布局系统、JSON UI、动画与更多图形后端。

当前仓库已经包含：

- `bkui`：核心库
- `demo`：示例程序
- `resources`：图片、字体、Shader 与演示资源
- `docs`：实施文档、目标说明与编译说明
- `library`：用于结构参考和兼容实现对照的外部代码库

## 项目技术栈

### 语言与构建

- C++20
- CMake 3.22+
- Windows 批处理脚本：`build_windows.bat`
- MSYS2 / Bash 脚本：`build_switch.sh`

### 工程分层

当前工程按照下面的方向组织：

```text
Demo
  -> bkui core / resource / platform / rhi
  -> backend
  -> system
```

`bkui/src` 目前主要分为：

- `core`：基础类型、通用逻辑
- `resource`：文件系统、图片 / SVG / 字体等资源访问与加载
- `platform`：按平台拆分的输入、视频、字体、音频、IME 等实现
- `rhi`：图形资源与命令抽象
- `backend`：具体图形后端实现

### 平台与图形后端

当前已接入或已使用的能力：

- Windows / 桌面：
  - SDL3 平台层
  - OpenGL 图形后端
- Nintendo Switch：
  - libnx 平台层
  - deko3d 图形后端
  - 系统共享字体接入

### 资源系统

资源目录位于 `resources`，当前包含：

- `images`：PNG / SVG 图片资源
- `font`：演示字体资源
- `material`：Material Icons 字体
- `shaders`：OpenGL / deko3d 相关 Shader 资源
- `audio` / `video` / `text` / `themes` / `ui`：预留扩展目录

Windows 下资源会复制到可执行文件旁边，Switch 下资源会打包进 `romfs`。

## 第三方库分类

下面按“当前工程已接入”与“仓库中包含或参考”区分说明。

### 当前工程已接入

#### 平台与系统

- SDL3：桌面平台窗口、事件与上下文
- libnx：Nintendo Switch 平台能力
- deko3d：Switch 图形后端

#### 图形与渲染

- OpenGL：桌面图形后端
- stb：PNG 解码、字体光栅化等轻量能力

#### 资源与文件系统

- PhysicsFS（PhysFS）：统一虚拟文件系统与资源读取
- zlib：压缩相关基础依赖

### 仓库中包含或预留接入的三方库

这些库已经出现在仓库目录中，用于后续扩展、平台兼容或能力预研：

- FreeType：字体解析与光栅化
- HarfBuzz：复杂文本排版与 shaping
- GLM：数学库
- msdfgen：距离场字体 / 图形相关能力
- nlohmann/json：JSON 解析
- ThorVG：矢量图形 / SVG 方向能力
- BS thread-pool：线程池

### `library` 目录中的参考库

`library` 目录中包含了 Borealis 相关代码与其外部依赖，当前主要作为以下用途：

- 平台目录结构参考
- Switch 平台实现对照
- deko3d / 字体 / 视频等模块兼容思路参考

这部分并不等同于当前 `bkui` 运行时全部直接依赖，但对项目演进有明显参考价值。

## 构建说明

### Windows

在项目根目录执行：

```bat
build_windows.bat
```

### Nintendo Switch

在已配置 devkitPro / libnx / deko3d 环境后执行：

```bash
./build_switch.sh
```

Switch 构建会输出 `nro`，并在打包阶段处理 `romfs` 资源与 deko3d Shader。

## Demo 内容

当前 demo 主要用于验证以下能力：

- 基础平台初始化
- OpenGL / deko3d 后端可用性
- 资源文件读取
- PNG / SVG 图片显示
- 中文与多语言文字渲染
- Material Icons 图标字体显示

## 致谢

感谢以下开源项目与生态为本项目提供基础能力、实现参考或工程启发：

- [SDL](https://www.libsdl.org/)
- [PhysicsFS](https://www.physfs.org/)
- [stb](https://github.com/nothings/stb)
- [FreeType](https://freetype.org/)
- [HarfBuzz](https://harfbuzz.github.io/)
- [GLM](https://github.com/g-truc/glm)
- [msdfgen](https://github.com/Chlumsky/msdfgen)
- [ThorVG](https://www.thorvg.org/)
- [nlohmann/json](https://github.com/nlohmann/json)
- [zlib](https://zlib.net/)
- [BS thread-pool](https://github.com/bshoshany/thread-pool)
- [devkitPro](https://devkitpro.org/)
- [libnx](https://github.com/switchbrew/libnx)
- [deko3d](https://github.com/devkitPro/deko3d)
- [Borealis](https://github.com/natinusala/borealis)
- [Material Icons](https://fonts.google.com/icons)

同时感谢上述项目的维护者、贡献者与社区，让跨平台图形与 UI 工程能够建立在成熟、可靠的开源基础之上。

## 后续方向

结合 `docs/实施文档.md`，项目后续可继续推进：

- 更完整的 RHI 抽象
- 文本排版升级
- SVG 渲染质量与性能优化
- JSON 描述式 UI
- 布局系统与控件系统
- 动画系统
- Vulkan 等更多图形后端
