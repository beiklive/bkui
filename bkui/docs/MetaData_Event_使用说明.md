# MetaData 与 Event 使用说明

本文档说明 `bkui` 核心层中 `MetaData` 与 `Event` 的设计用途、常见用法和注意事项。

相关头文件：

- `bkui/include/bkui/core/MetaData.hpp`
- `bkui/include/bkui/core/Event.hpp`

## 1. MetaData

`MetaData` 是一个按字符串键存储任意类型数据的轻量容器，适合做：

- 控件扩展属性
- 节点附加状态
- 事件上下文数据
- 临时运行时参数表

### 1.1 特点

- 支持任意非引用类型
- 支持值类型、对象类型、move-only 类型
- 小对象优先走内联存储，减少堆分配
- 提供类型安全读取
- 支持按键覆盖、删除、清空

### 1.2 基本用法

```cpp
#include <bkui/core/MetaData.hpp>

bk::MetaData meta;

meta.Set("width", 1280);
meta.Set("height", 720);
meta.Set("title", std::string("BeikUI Demo"));
meta.Set("scale", 1.25f);

if (const int* width = meta.Get<int>("width"))
{
    // *width == 1280
}

if (meta.Is<std::string>("title"))
{
    const std::string* title = meta.Get<std::string>("title");
}
```

### 1.3 原地构造对象

当对象构造成本较高，或者你不想先构造临时对象再移动进去时，可以用 `Emplace`：

```cpp
#include <bkui/core/MetaData.hpp>

struct UserInfo
{
    std::string name;
    int age = 0;

    UserInfo(std::string userName, int userAge)
        : name(std::move(userName))
        , age(userAge)
    {
    }
};

bk::MetaData meta;
UserInfo& user = meta.Emplace<UserInfo>("user", "Alice", 18);
```

### 1.4 获取默认值

```cpp
bk::MetaData meta;
meta.Set("opacity", 0.85f);

const float opacity = meta.GetOr("opacity", 1.0f);
const int tabIndex = meta.GetOr("tabIndex", 0);
```

### 1.5 检查、删除与清空

```cpp
bk::MetaData meta;
meta.Set("visible", true);

if (meta.Contains("visible"))
{
    meta.Remove("visible");
}

meta.Clear();
```

### 1.6 存储 move-only 类型

`MetaData` 支持 `std::unique_ptr` 这种只能移动不能拷贝的类型：

```cpp
#include <memory>

bk::MetaData meta;
meta.Set("texture", std::make_unique<int>(42));

std::unique_ptr<int>* ptr = meta.Get<std::unique_ptr<int>>("texture");
```

注意：

- 如果 `MetaData` 内部包含 move-only 类型，整个 `MetaData` 对象不能安全做拷贝
- 当前实现会在拷贝时抛出异常，提示存在 move-only 条目

### 1.7 推荐使用场景

适合：

- 配置属性表
- 节点附加参数
- 非固定 schema 的小规模数据集合

不适合：

- 高频海量插入删除的大对象缓存
- 强 schema 的核心业务结构
- 需要跨线程无锁共享的大型数据仓库

## 2. Event

`Event` 是一个类似 Qt signal 的轻量事件系统，用于一个事件源通知多个监听者。

### 2.1 特点

- 支持多个订阅者
- 支持 lambda、函数对象、成员函数、const 成员函数
- 支持连接句柄断开
- 支持 RAII 自动断开
- 支持事件发射期间新增或移除监听
- 使用小对象优化函数包装，尽量减少分配

### 2.2 基本定义

```cpp
#include <bkui/core/Event.hpp>

bk::Event<void()> onReady;
bk::Event<void(int, int)> onResize;
bk::Event<void(const std::string&)> onTextChanged;
```

### 2.3 连接 lambda

```cpp
bk::Event<void(int)> onValueChanged;

bk::EventConnection connection = onValueChanged.Connect([](int value) {
    // 处理事件
});

onValueChanged.Emit(42);
```

也可以直接用函数调用语法：

```cpp
onValueChanged(100);
```

### 2.4 连接成员函数

```cpp
class DemoPanel
{
public:
    void HandleResize(int width, int height)
    {
    }
};

bk::Event<void(int, int)> onResize;
DemoPanel panel;

bk::EventConnection connection = onResize.Connect(&panel, &DemoPanel::HandleResize);
```

连接 `const` 成员函数也可以：

```cpp
class Logger
{
public:
    void Print(int value) const
    {
    }
};

Logger logger;
bk::Event<void(int)> onNumber;
bk::EventConnection connection = onNumber.Connect(&logger, &Logger::Print);
```

### 2.5 主动断开连接

```cpp
bk::Event<void()> onClose;

bk::EventConnection connection = onClose.Connect([]() {
});

connection.Disconnect();
```

### 2.6 使用 ScopedEventConnection

如果你希望对象析构时自动断开连接，推荐使用 `ScopedEventConnection`：

```cpp
class Controller
{
public:
    explicit Controller(bk::Event<void(int)>& event)
        : connection_(event.Connect([this](int value) {
            OnValue(value);
        }))
    {
    }

private:
    void OnValue(int value)
    {
    }

    bk::ScopedEventConnection connection_;
};
```

### 2.7 预留容量

如果你知道一个事件大概会有多少监听者，建议提前 `Reserve`：

```cpp
bk::Event<void()> onTick;
onTick.Reserve(16);
```

这样可以减少 `std::vector` 扩容次数。

### 2.8 发射期间增删监听

当前 `Event` 支持在事件回调执行过程中：

- 断开当前或其他连接
- 新增新的连接

新增连接不会插入当前这轮正在遍历的区间，而是延后到本轮发射结束后生效，这样可以避免遍历失效问题。

示例：

```cpp
bk::Event<void()> event;
bk::EventConnection first;

first = event.Connect([&]() {
    first.Disconnect();
    event.Connect([]() {
        // 这个监听会在下一次 Emit 时生效
    });
});

event.Emit();
event.Emit();
```

### 2.9 清空事件

```cpp
bk::Event<void()> onRefresh;
onRefresh.Clear();
```

### 2.10 查询状态

```cpp
bk::Event<void()> onUpdate;

const bool empty = onUpdate.Empty();
const std::size_t count = onUpdate.Size();
```

## 3. 组合使用示例

下面是一个把 `MetaData` 和 `Event` 组合起来的简单例子：

```cpp
#include <bkui/core/Event.hpp>
#include <bkui/core/MetaData.hpp>

class Node
{
public:
    bk::MetaData meta;
    bk::Event<void(const char* key)> onMetaChanged;

    template <typename T>
    void SetMeta(const char* key, T&& value)
    {
        meta.Set(key, std::forward<T>(value));
        onMetaChanged.Emit(key);
    }
};

Node node;
node.onMetaChanged.Connect([&](const char* key) {
    if (const int* width = node.meta.Get<int>(key))
    {
        // 响应 width 更新
    }
});

node.SetMeta("width", 300);
```

## 4. 使用建议

### MetaData 建议

- 稳定类型用固定结构体，不要全部塞进 `MetaData`
- `MetaData` 适合做边缘扩展数据，不适合替代核心模型
- 高频路径下尽量复用 key，避免不必要的字符串构造

### Event 建议

- 生命周期明确的对象优先使用 `ScopedEventConnection`
- 高频事件建议提前 `Reserve`
- 回调里尽量避免重量级逻辑，保持发射路径短

## 5. 头文件速查

```cpp
#include <bkui/core/MetaData.hpp>
#include <bkui/core/Event.hpp>
```

如果已经包含总头，也可以直接使用：

```cpp
#include <bkui/bkui.hpp>
```
