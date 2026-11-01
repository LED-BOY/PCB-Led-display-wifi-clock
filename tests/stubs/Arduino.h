#pragma once
#include <stdint.h>
#include <time.h>
#define IRAM_ATTR
#define DRAM_ATTR
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portENTER_CRITICAL_ISR(x) ((void)(x))
#define portEXIT_CRITICAL_ISR(x) ((void)(x))
struct hw_timer_t {};
inline hw_timer_t* timerBegin(unsigned) { static hw_timer_t t; return &t; }
inline void timerAttachInterrupt(hw_timer_t*,void (*)()) {}
inline void timerAlarm(hw_timer_t*,unsigned,bool,unsigned) {}
