#pragma once
using gpio_num_t = int;
constexpr int GPIO_MODE_INPUT = 0, GPIO_MODE_OUTPUT = 1, GPIO_FLOATING = 0;
extern int pinMode[32], pinLevel[32];
inline void gpio_set_direction(int p,int value) { pinMode[p]=value; }
inline void gpio_set_level(int p,int value) { pinLevel[p]=value; }
inline void gpio_set_pull_mode(int,int) {}
