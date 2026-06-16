#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace bk
{

template <typename Signature>
class Event;

template <typename Signature, std::size_t InlineSize = sizeof(void*) * 4>
class InplaceFunction;

template <typename R, typename... Args, std::size_t InlineSize>
class InplaceFunction<R(Args...), InlineSize>
{
public:
    InplaceFunction() = default;

    template <typename Callable>
    InplaceFunction(Callable&& callable)
    {
        Emplace<std::decay_t<Callable>>(std::forward<Callable>(callable));
    }

    InplaceFunction(const InplaceFunction&) = delete;
    InplaceFunction& operator=(const InplaceFunction&) = delete;

    InplaceFunction(InplaceFunction&& other) noexcept
    {
        MoveFrom(std::move(other));
    }

    InplaceFunction& operator=(InplaceFunction&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            MoveFrom(std::move(other));
        }
        return *this;
    }

    ~InplaceFunction()
    {
        Reset();
    }

    R operator()(Args... args) const
    {
        return ops_->invoke(storage_, std::forward<Args>(args)...);
    }

private:
    struct Ops
    {
        R (*invoke)(const void*, Args&&...);
        void (*destroy)(void*) noexcept;
        void (*moveConstruct)(void*, void*) noexcept;
        std::size_t size = 0;
        std::size_t align = 0;
        bool inlineStorage = false;
    };

    template <typename Callable>
    static const Ops& GetOps() noexcept
    {
        static const Ops ops{
            [](const void* ptr, Args&&... args) -> R {
                return (*static_cast<const Callable*>(ptr))(std::forward<Args>(args)...);
            },
            [](void* ptr) noexcept { static_cast<Callable*>(ptr)->~Callable(); },
            [](void* src, void* dst) noexcept { new (dst) Callable(std::move(*static_cast<Callable*>(src))); },
            sizeof(Callable),
            alignof(Callable),
            sizeof(Callable) <= InlineSize && alignof(Callable) <= alignof(std::max_align_t) && std::is_nothrow_move_constructible_v<Callable>,
        };
        return ops;
    }

    template <typename Callable, typename... CtorArgs>
    void Emplace(CtorArgs&&... args)
    {
        const Ops& ops = GetOps<Callable>();
        ops_ = &ops;

        if (ops.inlineStorage)
        {
            storage_ = InlinePtr();
            new (storage_) Callable(std::forward<CtorArgs>(args)...);
        }
        else
        {
            storage_ = ::operator new(ops.size, std::align_val_t(ops.align));
            new (storage_) Callable(std::forward<CtorArgs>(args)...);
        }
    }

    void Reset()
    {
        if (ops_ == nullptr)
        {
            return;
        }

        ops_->destroy(storage_);
        if (!ops_->inlineStorage)
        {
            ::operator delete(storage_, std::align_val_t(ops_->align));
        }

        ops_ = nullptr;
        storage_ = nullptr;
    }

    void MoveFrom(InplaceFunction&& other) noexcept
    {
        if (other.ops_ == nullptr)
        {
            return;
        }

        ops_ = other.ops_;
        if (ops_->inlineStorage)
        {
            storage_ = InlinePtr();
            ops_->moveConstruct(other.storage_, storage_);
            ops_->destroy(other.storage_);
        }
        else
        {
            storage_ = other.storage_;
        }

        other.ops_ = nullptr;
        other.storage_ = nullptr;
    }

    void* InlinePtr() noexcept
    {
        return static_cast<void*>(inlineStorage_);
    }

    const Ops* ops_ = nullptr;
    void* storage_ = nullptr;
    alignas(std::max_align_t) std::byte inlineStorage_[InlineSize]{};
};

/// 轻量事件连接句柄，可用于主动断开监听。
class EventConnection
{
public:
    EventConnection() = default;

    /// 断开当前连接。
    void Disconnect()
    {
        if (state_.disconnect != nullptr)
        {
            state_.disconnect(state_.state, id_);
            state_.disconnect = nullptr;
            id_ = 0;
        }
    }

    /// 查询连接是否仍然有效。
    [[nodiscard]] bool Connected() const
    {
        return state_.disconnect != nullptr;
    }

private:
    template <typename>
    friend class Event;

    struct SharedDisconnectState
    {
        std::weak_ptr<void> state;
        void (*disconnect)(const std::weak_ptr<void>&, std::uint64_t) = nullptr;
    };

    EventConnection(SharedDisconnectState state, std::uint64_t id)
        : state_(std::move(state))
        , id_(id)
    {
    }

    SharedDisconnectState state_{};
    std::uint64_t id_ = 0;
};

/// RAII 连接句柄，析构时自动断开事件连接。
class ScopedEventConnection
{
public:
    ScopedEventConnection() = default;

    explicit ScopedEventConnection(EventConnection connection)
        : connection_(std::move(connection))
    {
    }

    ScopedEventConnection(const ScopedEventConnection&) = delete;
    ScopedEventConnection& operator=(const ScopedEventConnection&) = delete;

    ScopedEventConnection(ScopedEventConnection&& other) noexcept
        : connection_(std::move(other.connection_))
    {
    }

    ScopedEventConnection& operator=(ScopedEventConnection&& other) noexcept
    {
        if (this != &other)
        {
            Disconnect();
            connection_ = std::move(other.connection_);
        }
        return *this;
    }

    ~ScopedEventConnection()
    {
        Disconnect();
    }

    /// 主动断开连接。
    void Disconnect()
    {
        connection_.Disconnect();
    }

    /// 查询连接是否仍然有效。
    [[nodiscard]] bool Connected() const
    {
        return connection_.Connected();
    }

private:
    EventConnection connection_;
};

/// 类似 Qt 信号的轻量事件类，支持多订阅者连接与断开。
template <typename R, typename... Args>
class Event<R(Args...)>
{
public:
    Event() = default;

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    /// 预留连接槽位，减少频繁扩容带来的分配。
    void Reserve(std::size_t capacity)
    {
        EnsureState();
        state_->slots.reserve(capacity);
        state_->pendingSlots.reserve(capacity);
    }

    /// 连接任意可调用对象。
    template <typename Callable>
    EventConnection Connect(Callable&& callable)
    {
        using CallableType = std::decay_t<Callable>;
        static_assert(std::is_invocable_r_v<R, CallableType&, Args...>, "Callable signature does not match event.");

        EnsureState();

        const std::uint64_t id = ++state_->nextId;
        Slot slot{
            id,
            true,
            InplaceFunction<R(Args...)>(std::forward<Callable>(callable)),
        };

        if (state_->dispatchDepth == 0)
        {
            state_->slots.push_back(std::move(slot));
        }
        else
        {
            state_->pendingSlots.push_back(std::move(slot));
        }
        state_->activeCount += 1;

        return EventConnection(
            typename EventConnection::SharedDisconnectState{
                std::weak_ptr<void>(state_),
                [](const std::weak_ptr<void>& opaqueState, std::uint64_t connectionId) {
                    if (const auto locked = opaqueState.lock())
                    {
                        static_cast<State*>(locked.get())->Disconnect(connectionId);
                    }
                },
            },
            id);
    }

    /// 连接对象成员函数。
    template <typename TObject>
    EventConnection Connect(TObject* object, R (TObject::*method)(Args...))
    {
        return Connect([object, method](Args... args) -> R {
            return (object->*method)(std::forward<Args>(args)...);
        });
    }

    /// 连接 const 对象成员函数。
    template <typename TObject>
    EventConnection Connect(const TObject* object, R (TObject::*method)(Args...) const)
    {
        return Connect([object, method](Args... args) -> R {
            return (object->*method)(std::forward<Args>(args)...);
        });
    }

    /// 移除指定连接。
    void Disconnect(const EventConnection& connection)
    {
        if (state_ != nullptr)
        {
            state_->Disconnect(connection.id_);
        }
    }

    /// 清空所有连接。
    void Clear()
    {
        if (state_ == nullptr)
        {
            return;
        }

        state_->activeCount = 0;
        state_->pendingSlots.clear();

        if (state_->dispatchDepth == 0)
        {
            state_->slots.clear();
            state_->pendingCompact = false;
        }
        else
        {
            for (Slot& slot : state_->slots)
            {
                slot.active = false;
            }
            state_->pendingCompact = true;
        }
    }

    /// 返回当前有效连接数量。
    [[nodiscard]] std::size_t Size() const
    {
        return state_ != nullptr ? state_->activeCount : 0;
    }

    /// 返回当前是否没有任何订阅者。
    [[nodiscard]] bool Empty() const
    {
        return Size() == 0;
    }

    /// 触发事件。
    void Emit(Args... args) const
    {
        if (state_ == nullptr || state_->activeCount == 0)
        {
            return;
        }

        state_->dispatchDepth += 1;
        const std::size_t stableCount = state_->slots.size();
        for (std::size_t i = 0; i < stableCount; ++i)
        {
            Slot& slot = state_->slots[i];
            if (slot.active)
            {
                slot.callback(args...);
            }
        }
        state_->dispatchDepth -= 1;

        if (state_->dispatchDepth == 0)
        {
            if (state_->pendingCompact)
            {
                state_->Compact();
            }
            state_->FlushPending();
        }
    }

    /// 函数调用语法糖。
    void operator()(Args... args) const
    {
        Emit(std::forward<Args>(args)...);
    }

private:
    struct Slot
    {
        std::uint64_t id = 0;
        bool active = false;
        InplaceFunction<R(Args...)> callback;
    };

    struct State
    {
        void Disconnect(std::uint64_t id)
        {
            if (DisconnectFrom(slots, id))
            {
                return;
            }

            DisconnectFrom(pendingSlots, id);
        }

        bool DisconnectFrom(std::vector<Slot>& container, std::uint64_t id)
        {
            for (Slot& slot : container)
            {
                if (slot.id == id && slot.active)
                {
                    slot.active = false;
                    if (activeCount > 0)
                    {
                        activeCount -= 1;
                    }

                    if (dispatchDepth == 0)
                    {
                        Compact();
                    }
                    else
                    {
                        pendingCompact = true;
                    }
                    return true;
                }
            }
            return false;
        }

        void Compact()
        {
            slots.erase(
                std::remove_if(
                    slots.begin(),
                    slots.end(),
                    [](const Slot& slot) {
                        return !slot.active;
                    }),
                slots.end());
            pendingSlots.erase(
                std::remove_if(
                    pendingSlots.begin(),
                    pendingSlots.end(),
                    [](const Slot& slot) {
                        return !slot.active;
                    }),
                pendingSlots.end());
            pendingCompact = false;
        }

        void FlushPending()
        {
            if (pendingSlots.empty())
            {
                return;
            }

            for (Slot& slot : pendingSlots)
            {
                if (slot.active)
                {
                    slots.push_back(std::move(slot));
                }
            }
            pendingSlots.clear();
        }

        std::vector<Slot> slots;
        std::vector<Slot> pendingSlots;
        std::uint64_t nextId = 0;
        std::size_t activeCount = 0;
        std::size_t dispatchDepth = 0;
        bool pendingCompact = false;
    };

    void EnsureState()
    {
        if (state_ == nullptr)
        {
            state_ = std::make_shared<State>();
        }
    }

    std::shared_ptr<State> state_;
};

}
