#pragma once

// try to implement a sizeof(SharedPtr) == 8 SharedPtr 

#include <inttypes.h>
#include <atomic>
#include <memory>
#include <type_traits>
template <typename T>
class WeakPtr;

template <typename T>
class EnableSharedFromThis;


template <class T> 
class SharedPtrAllocater{

}; 

template <class T>
class RefCount;

// 判断是否继承了 EnableSharedFromThis 的模板类
template <typename T, typename = void>
struct __has_shared_from_this_base : std::false_type
{
};

// 当 T 继承自 EnableSharedFromThis<T> 时，启用特化
template <typename T>
struct __has_shared_from_this_base<
    T, std::void_t<typename std::enable_if<std::is_base_of<EnableSharedFromThis<T>, T>::value>::type>> : std::true_type
{
};

template <typename T, typename R = typename std::remove_cv<T>::type>
typename std::enable_if<__has_shared_from_this_base<R>::value>::type
__enable_shared_from_this_with(T *p, RefCount<T> *ref);

template <typename T, typename R = typename std::remove_cv<T>::type>
typename std::enable_if<!__has_shared_from_this_base<R>::value>::type
__enable_shared_from_this_with(T *p, RefCount<T> *ref)
{ 
}

// 引用计数类
template <class T>
class RefCount
{
public:
    template <class... Args>
    RefCount(Args &&... args) : count(0), object(std::forward<Args>(args)...)
    {
        __enable_shared_from_this_with<T>(&object, this);
    }

    void add_ref()
    {
        ++count;
    }

    int release_ref()
    {
        return --count;
    }

    int get_count() const
    {
        return count;
    }

    T &get()
    {
        return object;
    }
    const T &get() const
    {
        return object;
    }

private:
    std::atomic<int> count;
    T object;
};

// 自定义 shared_ptr 类
template <typename T>
class SharedPtr
{
public:

	//forbiden to create from raw pointer 
	SharedPtr(T *) = delete; 
	SharedPtr(const T *) = delete; 

    SharedPtr():ref_count(nullptr){
    } 

    template <class... Args>
    SharedPtr init (Args &&... args)   
    {
        ref_count = (new RefCount<T>(std::forward<Args>(args)...)); 
        ref_count->add_ref();
        return *this; 
    }

    template <class... Args>
    SharedPtr init (Args &&... args)  const 
    {
        ref_count = (new RefCount<T>(std::forward<Args>(args)...)); 
        ref_count->add_ref();
        return *this; 
    }

    SharedPtr(const SharedPtr &other) : ref_count(other.ref_count)
    {
        ref_count->add_ref();
    }

    SharedPtr(const WeakPtr<T> &weakPtr)
    {
        ref_count = weakPtr.ref_count;
        if (ref_count && ref_count->get_count() > 0)
        {
            ref_count->add_ref();
        }
        else
        {
            ref_count = nullptr;
        }
    }

    SharedPtr(SharedPtr &&other) noexcept : ref_count(other.ref_count)
    {
        other.ref_count = nullptr;
    }

    SharedPtr &operator=(const SharedPtr &other)
    {
        if (this != &other)
        {
            release();
            ref_count = other.ref_count;
            ref_count->add_ref();
        }
        return *this;
    }

    SharedPtr &operator=(SharedPtr &&other) noexcept
    {
        if (this != &other)
        {
            release();
            ref_count = other.ref_count;
            other.ref_count = nullptr;
        }
        return *this;
    }

    ~SharedPtr()
    {
        release();
    }

    T &operator*() const
    {
        return ref_count->object;
    }

    T *operator->() const
    {
        return &(ref_count->get());
    }

    explicit operator bool() const
    {
        return ref_count != nullptr;
    }

    int use_count() const
    {
        return ref_count ? ref_count->get_count() : 0;
    }

    // private:

    void release()
    {
        if (ref_count && ref_count->release_ref() == 0)
        {
            //printf("release from sheard ptr %d\n", ref_count->get_count());
            delete ref_count;
        }
    }
	// void release() const 
    // {
    //     if (ref_count && ref_count->release_ref() == 0)
    //     {
    //         printf("release from sheard ptr %d\n", ref_count->get_count());
    //         delete ref_count;
    //     }
    // }
    mutable RefCount<T> *ref_count = nullptr;
    friend class WeakPtr<T>;
};

template <typename T>
class WeakPtr
{
public:
    WeakPtr() : ref_count(nullptr) {}

    WeakPtr(const SharedPtr<T> &shared_ptr) : ref_count(shared_ptr.ref_count)
    {
        if (ref_count)
        {
        }
    }

    WeakPtr(const WeakPtr &other) : ref_count(other.ref_count)
    {
    }

    ~WeakPtr()
    {
        release();
    }

    WeakPtr &operator=(const WeakPtr &other)
    {
        if (this != &other)
        {
            release();

            ref_count = other.ref_count;
            if (ref_count)
            {
            }
        }
        return *this;
    }

    SharedPtr<T> lock() const
    {
        if (ref_count && ref_count->get_count() > 0)
        {
            return SharedPtr<T>(*this);
        }
        else
        {
            return SharedPtr<T>();
        }
    }

    // private:
    RefCount<T> *ref_count = nullptr;

    void release()
    {
        if (ref_count)
        {
            if (ref_count->get_count() == 0)
            {         
            }
        }
    }

    friend class SharedPtr<T>;
};

template <typename T>
class EnableSharedFromThis
{
public:
    SharedPtr<const T> shared_from_this() const
    {
        auto sp = weak_from_this().lock();
        if (!sp)
        {
            throw std::bad_weak_ptr();
        }
        return sp;
    }

    SharedPtr<T> shared_from_this()
    {
        auto sp = weak_from_this().lock();
        if (!sp)
        {
            throw std::bad_weak_ptr();
        }
        return sp;
    }

    WeakPtr<T> weak_from_this() const
    {
        return WeakPtr<T>(weak_this.lock());
    }

protected:
    EnableSharedFromThis() = default;
    EnableSharedFromThis(const EnableSharedFromThis &) = delete;
    EnableSharedFromThis &operator=(const EnableSharedFromThis &) = delete;

    template <typename U>
    friend class SharedPtr;

    template <typename U>
    friend class RefCount;

public:
    mutable WeakPtr<T> weak_this;
};

template <typename T, typename R >
typename std::enable_if<__has_shared_from_this_base<R>::value>::type
__enable_shared_from_this_with(T *p, RefCount<T> *ref)
{
    EnableSharedFromThis<T> *base = static_cast<EnableSharedFromThis<T> *>(p);
    base->weak_this.ref_count = ref;  
}

template <class T, class... Args>
SharedPtr<T> create_shared(Args && ... args)
{
    return  SharedPtr<T>().init(std::forward<Args>(args)...); 
}
