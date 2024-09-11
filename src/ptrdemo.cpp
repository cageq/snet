#include <iostream>
#include <memory>
#include <cassert>

template <typename T>
class MyWeakPtr;  // 前向声明

template <typename T>
class MySharedPtr {
 
public:
    T* ptr;
    int* ref_count;
    int* weak_count;

    explicit MySharedPtr(T* p = nullptr)
        : ptr(p), ref_count(new int(1)), weak_count(new int(0)) {}

    MySharedPtr(const MySharedPtr<T>& other)
        : ptr(other.ptr), ref_count(other.ref_count), weak_count(other.weak_count) {
        ++(*ref_count);
    }

    MySharedPtr<T>& operator=(const MySharedPtr<T>& other) {
        if (this != &other) {
            release();
            ptr = other.ptr;
            ref_count = other.ref_count;
            weak_count = other.weak_count;
            ++(*ref_count);
        }
        return *this;
    }

    ~MySharedPtr() {
        release();
    }

    T* get() const {
        return ptr;
    }

    MyWeakPtr<T> weak_from_this() const {
        return MyWeakPtr<T>(*this);
    }

    std::shared_ptr<T> shared_from_this() const {
        return weak_from_this().lock();
    }

private:
    void release() {
        if (--(*ref_count) == 0) {
            delete ptr;
            if (*weak_count == 0) {
                delete ref_count;
                delete weak_count;
            }
        }
    }
};

template <typename T>
class MyWeakPtr {
 
public:
    T* ptr;
    int* ref_count;
    int* weak_count;

public:
    MyWeakPtr(const MySharedPtr<T>& shared_ptr)
        : ptr(shared_ptr.get()), ref_count(shared_ptr.ref_count), weak_count(shared_ptr.weak_count) {
        ++(*weak_count);
    }

    MyWeakPtr(const MyWeakPtr<T>& other)
        : ptr(other.ptr), ref_count(other.ref_count), weak_count(other.weak_count) {
        ++(*weak_count);
    }

    MyWeakPtr<T>& operator=(const MyWeakPtr<T>& other) {
        if (this != &other) {
            release();
            ptr = other.ptr;
            ref_count = other.ref_count;
            weak_count = other.weak_count;
            ++(*weak_count);
        }
        return *this;
    }

    ~MyWeakPtr() {
        release();
    }

    std::shared_ptr<T> lock() const {
        if (*ref_count > 0) {
            return std::shared_ptr<T>(*this); // Create shared_ptr to get strong reference
        } else {
            return std::shared_ptr<T>(); // Return an empty shared_ptr
        }
    }

private:
    void release() {
        if (--(*weak_count) == 0 && *ref_count == 0) {
            delete ref_count;
            delete weak_count;
        }
    }
};

// Base class for enable_shared_from_this
template <typename T>
class MyEnableSharedFromThis {
public:
    std::shared_ptr<T> shared_from_this() {
        auto sp = weak_ptr.lock();
        if (!sp) {
            throw std::bad_weak_ptr();
        }
        return sp;
    } 

    mutable MyWeakPtr<T> weak_ptr; 
};

// Derived class example
class MyClass : public MyEnableSharedFromThis<MyClass> {
public:
    void print() {
        std::cout << "MyClass instance" << std::endl;
    }
};

int main() {
    auto sp1 = MySharedPtr<MyClass>(new MyClass());
    auto sp2 = sp1.shared_from_this(); // Use enable_shared_from_this functionality
    sp2->print();

    return 0;
}