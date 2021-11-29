#pragma once

#include <cstddef>
#include <memory>
#include <utility>

template <typename T>
class shared_ptr;

template<typename T>
class weak_ptr;

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args);


class control_block {
  public:
    control_block() : counter(1), weak_counter(0) {}

    virtual ~control_block() = default ;

    virtual void delete_object() = 0;

    template <typename Y>
    friend class shared_ptr;

    template <typename Y>
    friend class weak_ptr;

private:

    void inc_ref();

    void dec_ref();

    void inc_weak_ref();

    void dec_weak_ref();

    int get_strong_count();

    bool object_deleted() const noexcept;

    size_t counter;
    size_t weak_counter;
};

template <typename T, typename D>
class regular_control_block : public control_block, D {
  public:
    regular_control_block(T* ptr_, D d) : ptr(ptr_), D(std::move(d)) {};

    ~regular_control_block() {}

    template <typename Y>
    friend class shared_ptr;

  private:

    void delete_object() override {
        static_cast<D&>(*this)(ptr);
    }

    T* ptr;
};



template <typename T>
class inplace_control_block : public control_block {
  public:

    template <typename... Args>
    inplace_control_block(Args&&... args) {
        new (&obj) T(std::forward<Args>(args)...);
    }

    ~inplace_control_block() {};

    template <typename Y>
    friend class shared_ptr;

  private:

    void delete_object() override {
        reinterpret_cast<T*>(&obj)->~T();
    }

    T* get() {
        return reinterpret_cast<T*>(&obj);
    }

    std::aligned_storage_t<sizeof(T), alignof(T)> obj;
};

template <typename T>
class shared_ptr {
  public:

    shared_ptr() noexcept {}

    shared_ptr(std::nullptr_t) noexcept {}

    template <typename Y, typename D = std::default_delete<Y>,
        typename = std::enable_if_t<std::is_convertible_v<Y, T>>>
    explicit shared_ptr(Y* ptr_, D&& deleter = D()) noexcept {
        try {
            block =
                new regular_control_block<Y, D>(ptr_, std::forward<D>(deleter));
            ptr = ptr_;
        } catch (...) {
            deleter.operator()(ptr_);
            throw;
        }
    }

    template<class Y>
    shared_ptr(const shared_ptr<Y> &r, T* ptr_) noexcept
        : block(r.block), ptr(ptr_){
        block->inc_ref();
    }

    shared_ptr(const shared_ptr& other) noexcept
        : block(other.block), ptr(other.ptr) {
        if (block) {
            block->inc_ref();
        }
    }

    shared_ptr(shared_ptr&& other) noexcept
        : block(other.block), ptr(other.ptr) {
        other.block = nullptr;
        other.ptr = nullptr;
    }

    shared_ptr& operator=(const shared_ptr& other) noexcept {
        if (this == &other) {
            return *this;
        }
        shared_ptr<T>(other).swap(*this);
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& other) noexcept {
        shared_ptr<T>(std::move(other)).swap(*this);
        return *this;
    }

    template <typename Y,
        typename = std::enable_if_t<std::is_convertible_v<T, Y>>>
    operator shared_ptr<Y>() noexcept {
        auto result = shared_ptr<Y>(block, static_cast<Y*>(ptr));
        block->inc_ref();
        return result;
    }

    T* get() const noexcept {
        return ptr;
    };

    operator bool() const noexcept {
        return (get() != nullptr);
    }
    T& operator*() const noexcept {
        return *get();
    }
    T* operator->() const noexcept {
        return get();
    }

    void swap(shared_ptr<T> &other) noexcept {
        std::swap(block, other.block);
        std::swap(ptr, other.ptr);
    }

    std::size_t use_count() const noexcept {
        return (block == nullptr ? 0 : block->get_strong_count());
    }

    void reset() noexcept {
        shared_ptr<T>().swap(*this);
    }

    template <typename X, typename D = std::default_delete<X>,
        typename = std::enable_if_t<std::is_convertible_v<X, T>>>
    void reset(X* new_ptr, D&& deleter = D()) noexcept {
        shared_ptr<T>(new_ptr, std::forward<D>(deleter)).swap(*this);
    }

    ~shared_ptr() {
        if (block) {
            block->dec_ref();
        }
    }

    friend weak_ptr<T>;

    template <class Y>
    friend class shared_ptr;

    template <typename Y, typename... Args>
    friend shared_ptr<Y> make_shared(Args&&... args);

  private:

    shared_ptr(inplace_control_block<T> *other_block) noexcept
    : block(other_block), ptr(other_block->get()) {}

    shared_ptr(control_block *other_block, T* ptr_) noexcept
    : block(other_block), ptr(ptr_) {}

    control_block* block = nullptr;
    T* ptr = nullptr;
};

template <typename T>
bool operator==(const shared_ptr<T>& a, const shared_ptr<T> &b) noexcept {
    return a.get() == b.get();
}

template <typename T>
bool operator!=(const shared_ptr<T>& a, const shared_ptr<T> &b) noexcept {
    return a.get() != b.get();
}

template <typename T>
bool operator==(const std::nullptr_t &a, const shared_ptr<T> &b) noexcept {
    return b.get() == nullptr;
}

template <typename T>
bool operator==(const shared_ptr<T> &a, const std::nullptr_t &b) noexcept {
    return a.get() == nullptr;
}

template <typename T>
bool operator!=(const std::nullptr_t &a, const shared_ptr<T> &b) noexcept {
    return b.get() != nullptr;
}

template <typename T>
bool operator!=(const shared_ptr<T> &a, const std::nullptr_t &b) noexcept {
    return a.get() != nullptr;
}

template <typename T>
class weak_ptr {
  public:
    weak_ptr() noexcept {}

    weak_ptr(const weak_ptr<T> &other) noexcept
        : block(other.block), ptr(other.ptr) {
        if (block) {
            block->inc_weak_ref();
        }
    }

    weak_ptr& operator=(const weak_ptr<T> &other) noexcept {
        if (&other == this) {
            return *this;
        }
        weak_ptr<T> tmp(other);
        swap(tmp);
        return *this;
    }

    weak_ptr(weak_ptr<T> &&other) noexcept
        : block(other.block), ptr(other.ptr) {
        other.block = nullptr;
        other.ptr = nullptr;
    }

    weak_ptr& operator=(weak_ptr<T> &&other) noexcept {
        weak_ptr<T>(std::move(other)).swap(*this);
        return *this;
    }

    weak_ptr(const shared_ptr<T>& other) noexcept
        : block(other.block), ptr(other.ptr) {
        if (block) {
            block->inc_weak_ref();
        }
    }

    void swap(weak_ptr<T> &other) noexcept {
        std::swap(block, other.block);
        std::swap(ptr, other.ptr);
    }


    weak_ptr& operator=(const shared_ptr<T>& other) noexcept {
        weak_ptr<T> tmp(other);
        swap(tmp);
        return *this;
    }

    shared_ptr<T> lock() const noexcept {
        if (block == nullptr || block->object_deleted()) {
            return shared_ptr<T>();
        }
        block->inc_ref();
        return shared_ptr<T>(block, ptr);
    }

    ~weak_ptr() {
        if (block) {
            block->dec_weak_ref();
        }
    }

  private:
    control_block* block = nullptr;
    T *ptr = nullptr;
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    return shared_ptr<T>(
        new inplace_control_block<T>(std::forward<Args>(args)...));
}
