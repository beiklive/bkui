#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace bk
{

/// 通用元数据容器，支持按键存取任意类型。
class MetaData
{
public:
    MetaData() = default;
    MetaData(const MetaData&) = default;
    MetaData(MetaData&&) noexcept = default;
    MetaData& operator=(const MetaData&) = default;
    MetaData& operator=(MetaData&&) noexcept = default;
    ~MetaData() = default;

    /// 设置一个键对应的值。
    template <typename T>
    void Set(std::string_view key, T&& value)
    {
        using StoredType = std::decay_t<T>;
        static_assert(!std::is_reference_v<StoredType>, "MetaData cannot store reference types.");

        Entry& entry = values_[std::string(key)];
        entry.template Emplace<StoredType>(std::forward<T>(value));
    }

    /// 原地构造一个键对应的值。
    template <typename T, typename... Args>
    T& Emplace(std::string_view key, Args&&... args)
    {
        static_assert(!std::is_reference_v<T>, "MetaData cannot store reference types.");

        Entry& entry = values_[std::string(key)];
        return entry.template Emplace<T>(std::forward<Args>(args)...);
    }

    /// 查询指定键是否存在。
    [[nodiscard]] bool Contains(std::string_view key) const
    {
        return values_.find(std::string(key)) != values_.end();
    }

    /// 查询指定键是否为目标类型。
    template <typename T>
    [[nodiscard]] bool Is(std::string_view key) const
    {
        const Entry* entry = FindEntry(key);
        return entry != nullptr && entry->template Is<T>();
    }

    /// 获取指定键的值，不存在或类型不匹配时返回空指针。
    template <typename T>
    [[nodiscard]] T* Get(std::string_view key)
    {
        Entry* entry = FindEntry(key);
        return entry != nullptr ? entry->template TryGet<T>() : nullptr;
    }

    /// 获取指定键的值，不存在或类型不匹配时返回空指针。
    template <typename T>
    [[nodiscard]] const T* Get(std::string_view key) const
    {
        const Entry* entry = FindEntry(key);
        return entry != nullptr ? entry->template TryGet<T>() : nullptr;
    }

    /// 获取指定键的值，不存在或类型不匹配时返回默认值。
    template <typename T>
    [[nodiscard]] std::decay_t<T> GetOr(std::string_view key, T&& fallback) const
    {
        using StoredType = std::decay_t<T>;
        if (const StoredType* value = Get<StoredType>(key))
        {
            return *value;
        }
        return static_cast<StoredType>(std::forward<T>(fallback));
    }

    /// 移除指定键。
    bool Remove(std::string_view key)
    {
        return values_.erase(std::string(key)) > 0;
    }

    /// 清空全部元数据。
    void Clear()
    {
        values_.clear();
    }

    /// 返回当前元数据项数量。
    [[nodiscard]] std::size_t Size() const
    {
        return values_.size();
    }

    /// 返回容器是否为空。
    [[nodiscard]] bool Empty() const
    {
        return values_.empty();
    }

private:
    static constexpr std::size_t InlineStorageSize = sizeof(void*) * 4;
    static constexpr std::size_t InlineStorageAlign = alignof(std::max_align_t);

    struct TypeOps
    {
        const std::type_info& (*Type)() noexcept;
        void (*Destroy)(void*) noexcept;
        void (*CopyConstruct)(const void*, void*);
        void (*MoveConstruct)(void*, void*) noexcept;
        std::size_t size = 0;
        std::size_t align = 0;
        bool inlineStorage = false;
        bool copyable = false;
    };

    template <typename T>
    static const TypeOps& Ops() noexcept
    {
        static const TypeOps ops{
            []() noexcept -> const std::type_info& { return typeid(T); },
            [](void* ptr) noexcept { static_cast<T*>(ptr)->~T(); },
            [](const void* src, void* dst) {
                if constexpr (std::is_copy_constructible_v<T>)
                {
                    new (dst) T(*static_cast<const T*>(src));
                }
            },
            [](void* src, void* dst) noexcept { new (dst) T(std::move(*static_cast<T*>(src))); },
            sizeof(T),
            alignof(T),
            sizeof(T) <= InlineStorageSize && alignof(T) <= InlineStorageAlign && std::is_nothrow_move_constructible_v<T>,
            std::is_copy_constructible_v<T>,
        };
        return ops;
    }

    class Entry
    {
    public:
        Entry() = default;

        Entry(const Entry& other)
        {
            CopyFrom(other);
        }

        Entry(Entry&& other) noexcept
        {
            MoveFrom(std::move(other));
        }

        Entry& operator=(const Entry& other)
        {
            if (this != &other)
            {
                Reset();
                CopyFrom(other);
            }
            return *this;
        }

        Entry& operator=(Entry&& other) noexcept
        {
            if (this != &other)
            {
                Reset();
                MoveFrom(std::move(other));
            }
            return *this;
        }

        ~Entry()
        {
            Reset();
        }

        template <typename T, typename... Args>
        T& Emplace(Args&&... args)
        {
            Reset();

            const TypeOps& ops = Ops<T>();
            ops_ = &ops;
            if (ops.inlineStorage)
            {
                storage_ = InlinePtr();
                new (storage_) T(std::forward<Args>(args)...);
            }
            else
            {
                storage_ = ::operator new(ops.size, std::align_val_t(ops.align));
                new (storage_) T(std::forward<Args>(args)...);
            }
            return *static_cast<T*>(storage_);
        }

        template <typename T>
        [[nodiscard]] bool Is() const
        {
            return ops_ != nullptr && ops_->Type() == typeid(T);
        }

        template <typename T>
        [[nodiscard]] T* TryGet()
        {
            return Is<T>() ? static_cast<T*>(storage_) : nullptr;
        }

        template <typename T>
        [[nodiscard]] const T* TryGet() const
        {
            return Is<T>() ? static_cast<const T*>(storage_) : nullptr;
        }

    private:
        void* InlinePtr() noexcept
        {
            return static_cast<void*>(inlineStorage_);
        }

        void Reset()
        {
            if (ops_ == nullptr)
            {
                return;
            }

            ops_->Destroy(storage_);
            if (!ops_->inlineStorage)
            {
                ::operator delete(storage_, std::align_val_t(ops_->align));
            }

            ops_ = nullptr;
            storage_ = nullptr;
        }

        void CopyFrom(const Entry& other)
        {
            if (other.ops_ == nullptr)
            {
                return;
            }

            if (!other.ops_->copyable)
            {
                throw std::runtime_error("MetaData entry type is move-only and cannot be copied.");
            }

            ops_ = other.ops_;
            if (ops_->inlineStorage)
            {
                storage_ = InlinePtr();
                ops_->CopyConstruct(other.storage_, storage_);
            }
            else
            {
                storage_ = ::operator new(ops_->size, std::align_val_t(ops_->align));
                ops_->CopyConstruct(other.storage_, storage_);
            }
        }

        void MoveFrom(Entry&& other) noexcept
        {
            if (other.ops_ == nullptr)
            {
                return;
            }

            ops_ = other.ops_;
            if (ops_->inlineStorage)
            {
                storage_ = InlinePtr();
                ops_->MoveConstruct(other.storage_, storage_);
                ops_->Destroy(other.storage_);
            }
            else
            {
                storage_ = other.storage_;
            }

            other.ops_ = nullptr;
            other.storage_ = nullptr;
        }

        const TypeOps* ops_ = nullptr;
        void* storage_ = nullptr;
        alignas(InlineStorageAlign) std::byte inlineStorage_[InlineStorageSize]{};
    };

    Entry* FindEntry(std::string_view key)
    {
        const auto it = values_.find(std::string(key));
        return it != values_.end() ? &it->second : nullptr;
    }

    const Entry* FindEntry(std::string_view key) const
    {
        const auto it = values_.find(std::string(key));
        return it != values_.end() ? &it->second : nullptr;
    }

    std::unordered_map<std::string, Entry> values_;
};

}
