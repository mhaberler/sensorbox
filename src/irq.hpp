#pragma once
#include "stdint.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "ringbuffer.hpp"

typedef struct {
    float timestamp;
    void *dev;
} irqmsg_t;

extern espidf::RingBuffer *measurements_queue;

extern uint32_t  hardirq_fail, softirq_fail, measurements_queue_full, commit_fail;

void IRAM_ATTR  irq_handler(void *param);
void run_baro_task(void* arg);
BaseType_t run_baro_task(void);
bool setup_queues(void);