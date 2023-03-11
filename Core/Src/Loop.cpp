//
// Created by marce on 20/01/2023.
//


#include "Loop.h"
#include "platform.h"
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <queue>

struct structure {
    int a;
    int b;
};

class Test {
public:
    Test() = default;
    void test()  {
        HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_14);
    };
};

/**
 * @brief  The application entry point.
 * @retval int
 */
extern "C" {

    void threadOne() {
        while(1) {
            HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_13);
            HAL_Delay(1000);
            printf("Hey 1\r\n");
            std::vector<int> v;
            std::unordered_map<int, int> m;
            v.push_back(1);
            v.push_back(2);
            m[1] = 1;
            m[2] = 2;
            std::queue<int> q;
            q.push(20);
            q.push(30);
            for (const auto& t : v){
                printf("%d\r\n", t);
            }
            uint32_t i = 0;
            std::vector<structure> ss;
            ss.push_back({1, 2});
            ss.push_back({3, 4});
            for (const auto& [a, b] : ss){
                printf("%d %d\r\n", a, b);
            }
        }
    }

    void threadTwo()
    {
        while (1)
        {
            Test test;
            test.test();
            HAL_Delay(500);
            HAL_GetTick();
        }
    }
}