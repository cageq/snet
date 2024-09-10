#include <stdio.h>
#include "shared_ptr.h"
#include <memory>

class StdClass : public std::enable_shared_from_this<StdClass> 
{
    public: 
    ~StdClass()
    {
        printf("destroy StdClass \n");
    }
    uint64_t index;

}; 

class MyClass : public EnableSharedFromThis<MyClass>
{
public:
    ~MyClass()
    {
        printf("destroy MyClass \n");
    }
    uint64_t index;
};

int main(int argc, char *argv[])
{

     SharedPtr<MyClass> myClass  = create_shared<MyClass>();

    printf("shared ptr size %lu\n", sizeof(myClass));
    printf("ref count %d\n", myClass.use_count());

    auto ptr2 = myClass;

    printf("ref count %d\n", myClass.use_count());

    auto ptr3 = myClass->shared_from_this();

    printf("ref count %d\n", myClass.use_count());


    // std::shared_ptr<StdClass> stdClass = std::make_shared<StdClass>(); 
    // auto ptr4 = stdClass->shared_from_this(); 
    //  printf("ref count %d\n", stdClass.use_count());
}