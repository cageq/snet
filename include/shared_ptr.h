#pragma once 
#include <inttypes.h>
#include <atomic>
#include <memory>
template<typename T>
class WeakPtr; 

// 引用计数类
template <class T> 
class RefCount {
public:
 
    template<class ... Args> 
    RefCount(Args ... args) : count(0), object(args ...) {}

    void add_ref() {
        ++count;
    }

    int release_ref() {
        return --count;
    }

    int get_count() const {
        return count;
    }

    T & get() {
        return object; 
    }
     const T & get() const {
        return object; 
    }

    
private:  
    std::atomic<int> count;  
    T  object; 
};


// 自定义 shared_ptr 类
template <typename T>
class SharedPtr {
public:
 
    template <class ... Args> 
    SharedPtr(Args ...  args ) : ref_count(new RefCount<T>(args ... )) {  
        ref_count->add_ref(); 
    }

 
    SharedPtr(const SharedPtr& other) :  ref_count(other.ref_count) {
        ref_count->add_ref();
    }


    
    SharedPtr(const WeakPtr<T>& weakPtr)  {
          ref_count = weakPtr.ref_count;
        if (ref_count && ref_count->get_count() > 0) { 
            ref_count->add_ref();
        } else { 
            ref_count = nullptr;
        }
    }
 
    SharedPtr(SharedPtr&& other) noexcept :   ref_count(other.ref_count) {
        other.ref_count = nullptr;
    }

 
    SharedPtr& operator=(const SharedPtr& other) {
        if (this != &other) {
            release();       
            ref_count = other.ref_count;
            ref_count->add_ref();
        }
        return *this;
    }

 
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            release();           
            ref_count = other.ref_count; 
            other.ref_count = nullptr;
        }
        return *this;
    }

 
    ~SharedPtr() {
        release();
    }

 
    T& operator*() const {
        return ref_count->object;
    }

 
    T* operator->() const {
        return &(ref_count->get());
    }

 
    explicit operator bool() const {
        return ref_count != nullptr;
    }

 
    int use_count() const {
        return ref_count ? ref_count->get_count() : 0;
    }

private:  
    RefCount<T> * ref_count = nullptr;  

    void release() {
        if (ref_count && ref_count->release_ref() == 0) {    
            printf("release from sheard ptr %d\n", ref_count->get_count()); 
            delete ref_count;
        }
    }

    friend class WeakPtr<T> ; 
};


template<typename T>
class WeakPtr {
public:
    WeakPtr() :   ref_count(nullptr) {}


    WeakPtr(const SharedPtr<T>& shared_ptr) :  ref_count(shared_ptr.ref_count) {
        if (ref_count) {
  
        }
    }

    
    WeakPtr(const WeakPtr& other) :   ref_count(other.ref_count) {       
    }

 
    ~WeakPtr() {
        release();
    }

    WeakPtr& operator=(const WeakPtr& other) {
        if (this != &other) {
            release();
 
            ref_count = other.ref_count;
            if (ref_count) {
             
            }
        }
        return *this;
    }
 
    SharedPtr<T> lock() const {
        if (ref_count && ref_count->get_count() > 0) {
            return SharedPtr<T>(*this);
        } else {
            return SharedPtr<T>();
        }
    }
 

//private:
    RefCount<T> * ref_count = nullptr;  

    void release() {
        if (ref_count) { 
            if (ref_count->get_count() == 0  ) {
                printf("release from weak ptr\n"); 
                //delete ref_count;
            }
        }
    }

 
    friend class SharedPtr<T>;
};

template <typename T>
class EnableSharedFromThis {
public:

 

    SharedPtr<const T> shared_from_this() const {
        auto sp = weak_from_this().lock();
        if (!sp) {
            throw std::bad_weak_ptr();
        }
        return sp;
    }


    SharedPtr<T> shared_from_this() {
        auto sp = weak_from_this().lock();
        if (!sp) {
            throw std::bad_weak_ptr();
        }
        return sp;
    }

    WeakPtr<T> weak_from_this() const {
        return WeakPtr<T>( weak_this.lock());
    }



protected:
 
    EnableSharedFromThis() = default; 
    EnableSharedFromThis(const EnableSharedFromThis&) = delete;
    EnableSharedFromThis& operator=(const EnableSharedFromThis&) = delete;


  

    template <typename U>
    friend class SharedPtr;

private: 
    mutable WeakPtr<T> weak_this;
};



template <class T, class ... Args > 
   SharedPtr<T>  create_shared(Args ... args){
        return  SharedPtr<T>(args ...); 
   }




