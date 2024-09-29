
/*
 * 测试 使用智能指针成员变量包裹 std::thread使用是否正常 
*/

#include <iostream>
#include <memory>
#include <thread>

#include <unistd.h>

class Test
{
public:
    void init()
    {
        th = std::make_unique<std::thread>([](){
            for (;;)
            {
                std::cout << "xxx" << std::endl;
                sleep(1);
            }
        });
    }

    void wait()
    {
        th->join();
    }

private:
    std::unique_ptr<std::thread> th;
};

int main()
{
    Test t;

    t.init();

    t.wait();
}