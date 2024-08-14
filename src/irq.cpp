#include "irq.hpp"
#include "esp32-hal.h"
#include "sensor.hpp"
#include "params.hpp"
#include "settings.hpp"
#include "broker.hpp"
#include "tickers.hpp"
#include "sensor.hpp"
#include "fmicro.h"
#include "pindefs.h"

#include <freertos/event_groups.h>

#undef TRACE_PINS

void ublox_read(void);
void nfc_poll(void);
void nfc_loop(void);

void battery_check(void);

espidf::RingBuffer *measurements_queue;
TaskHandle_t i2c_task;
SemaphoreHandle_t i2cMutex;
EventGroupHandle_t eventGroup;

EXT_TICKER(gps);
EXT_TICKER(deadman);


uint32_t hardirq_fail, softirq_fail, measurements_queue_full, commit_fail;

bool dps368_irq(dps_sensors_t * dev, const float &timestamp);
bool icm20948_irq(icm20948_t *dev, const float &timestamp);

bool setup_queues(void) {
    eventGroup = xEventGroupCreate();
    i2cMutex = xSemaphoreCreateMutex();
    if (i2cMutex == NULL) {
        log_e("xSemaphoreCreateMutex");
        return false;
    }
    measurements_queue = new espidf::RingBuffer();
    measurements_queue->create(MEASMT_QUEUELEN, RINGBUF_TYPE_NOSPLIT);
    return true;
}

BaseType_t run_i2c_task(void) {
#ifdef CONFIG_FREERTOS_UNICORE
    return xTaskCreate(run_i2c_task, "run_i2c_task", SOFTIRQ_STACKSIZE, NULL,
                       SOFTIRQ_PRIORITY, &i2c_task);
#else
    return xTaskCreatePinnedToCore(run_i2c_task, "run_i2c_task", BAROTASK_STACKSIZE, NULL,
                                   BAROTASK_PRIORITY, &i2c_task, BAROTASK_CORE);
#endif
}

// first level interrupt handler
// only notify 2nd level handler task passing any parameters
// call only from interruot context
void irq_handler(void *param) {
#ifdef USE_IRQ

    TOGGLE(TRIGGER1);
    irqmsg_t msg;
    msg.dev = param;
    msg.timestamp = fseconds();
    if (xQueueSendFromISR(irq_queue, (const void*) &msg, NULL) != pdTRUE) {
        // should clear IRQ so as to not get stuck
        // arduino-esp32 wont permit i2c i/o here
        hardirq_fail++;
    }
    TOGGLE(TRIGGER1);
#endif
}

// post a soft irq to the handler queue/task
// call only from userland context
void post_softirq(void *dev) {
#ifdef USE_IRQ

    TOGGLE(TRIGGER1);
    irqmsg_t msg = {
        .timestamp = fseconds(),
        .dev = dev
    };
    if (xQueueSend(irq_queue, (const void*) &msg, 0) != pdTRUE) {
        softirq_fail++;
    }
    TOGGLE(TRIGGER1);
#endif
}


static TickType_t prs_delay_ticks, temp_delay_ticks;
static uint32_t cycle;

// 2nd level interrupt handler
// runs in user context - can do Wire I/O, log etc
void run_i2c_task(void* arg) {
    // setup sensors
    log_d("acquire i2c mutex...");
    if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
        float prs_max_duration = 0.0;
        float temp_max_duration = 0.0;
        for (auto i = 0; i < num_dps_sensors; i++) {
            int n = 3;
            int16_t ret;
            while (n--) {
                ret = dps368_setup(i);
                if (ret == DPS__SUCCEEDED) {
                    break;
                }
                log_e("dps%d setup failed ret=%d - retrying", i, ret);
                delay(200);
            }
            if (ret != DPS__SUCCEEDED) {
                log_e("dps%d setup failed ret=%d - giving up", i, ret);
            }
            prs_max_duration = max(prs_max_duration, dps_sensors[i].prs_conversion_time);
            temp_max_duration = max(temp_max_duration, dps_sensors[i].temp_conversion_time);
            yield();
        }
        // Give back the mutex
        xSemaphoreGive(i2cMutex);
        prs_max_duration  += PRESSURE_DELAY_MARGIN;
        temp_max_duration += TEMP_DELAY_MARGIN;

        prs_delay_ticks = int(prs_max_duration*1000) / portTICK_PERIOD_MS;
        temp_delay_ticks = int(temp_max_duration*1000) / portTICK_PERIOD_MS;
        log_d("bar setup done.");
    }
    cycle = 0;

    // baro loop
    for (;;) {
        bool prs_measurement = (cycle++ & TEMP_COUNT_MASK);

        EventBits_t bits = xEventGroupWaitBits(
                               eventGroup,
                               EVENT_TRIGGER_DPS368 | EVENT_TRIGGER_ICM_20948 | EVENT_TRIGGER_M5STACK_IMU | EVENT_TRIGGER_NEO_M9N |
                               EVENT_TRIGGER_BATTERY | EVENT_TRIGGER_MICROPHONE,  // Waiting for these bits
                               pdTRUE,   // Clear the bits before returning
                               pdFALSE,  // Wait for any one bit to be set
                               1000 / portTICK_PERIOD_MS // Wait indefinitely   // FIXME baro interval
                           );
#ifdef IMU_SUPPORT
        if (bits & EVENT_TRIGGER_ICM_20948) {

            icm20948_irq(&imu_sensor, fseconds());
        }
#endif
        if (bits & EVENT_TRIGGER_ICM_20948) {
#ifdef UBLOX_SUPPORT
            ublox_read();
#endif
        }

        // vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
            int16_t ret;
            for (auto i = 0; i < num_dps_sensors; i++) {
                dps_sensors_t *dev = &dps_sensors[i];
                dev->start_time = fseconds();
                if (prs_measurement) {
                    ret = dev->sensor->startMeasurePressureOnce(dev->prs_osr);
                } else {
                    ret = dev->sensor->startMeasureTempOnce(dev->temp_osr);
                }
                if (ret != DPS__SUCCEEDED) {
                    log_e("startMeasureXnce %d %d", i, ret);
                    // FIXME failcounters
                    if (ret == DPS__FAIL_TOOBUSY) {
                        dev->sensor->standby();
                    }
                    continue;
                }
            }
            xSemaphoreGive(i2cMutex);
        }
        if (prs_measurement) {
            vTaskDelay(prs_delay_ticks);
        } else {
            vTaskDelay(temp_delay_ticks);
        }
        vTaskDelay(prs_measurement ? prs_delay_ticks : temp_delay_ticks);

        if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE) {
            for (auto i = 0; i < num_dps_sensors; i++) {
                dps_sensors_t *dev = &dps_sensors[i];
                float value;
                int16_t ret = dev->sensor->getSingleResult(value);
                if (ret != DPS__SUCCEEDED) {
                    log_e("getSingleResult %d %d", i, ret);
                    dev->sensor->standby();
                    continue;
                }
                if (!prs_measurement) {
                    dev->last_temperature = value;
                    continue;
                }
                // a sample is ready
                baroSample_t *bs = nullptr;
                size_t sz = sizeof(baroSample_t);
                if (measurements_queue->send_acquire((void **)&bs, sz, 0) != pdTRUE) {
                    measurements_queue_full++;
                    continue;
                    // TOGGLE(TRIGGER2);
                }
                bs->dev = dev;
                bs->timestamp = dev->start_time;
                bs->type  = SAMPLE_PRESSURE;
                bs->value = value / 100.0f;  // scale to hPa
                if (measurements_queue->send_complete(bs) != pdTRUE) {
                    commit_fail++;
                }
            }

            // if (bits & EVENT_TRIGGER_BATTERY) {
            //     battery_check();
            // }
            if (bits & EVENT_TRIGGER_NFC) {
                nfc_poll();
            }
        }
        xSemaphoreGive(i2cMutex);
    }
    // i2cMutex released
}



