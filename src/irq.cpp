#include "irq.hpp"
#include "esp32-hal.h"
#include "sensor.hpp"
#include "params.hpp"
#include "settings.hpp"
#include "broker.hpp"
#include "tickers.hpp"
#include "fmicro.h"
#include "pindefs.h"
#undef TRACE_PINS

void ublox_read(const void *dev);
void nfc_poll(void);
void battery_check(void);

QueueHandle_t irq_queue;
espidf::RingBuffer *measurements_queue;
TaskHandle_t baro_task;
SemaphoreHandle_t i2cMutex;
EXT_TICKER(gps);
EXT_TICKER(deadman);


uint32_t hardirq_fail, softirq_fail, measurements_queue_full, commit_fail;

bool dps368_irq(dps_sensors_t * dev, const float &timestamp);
bool icm20948_irq(icm20948_t *dev, const float &timestamp);

bool setup_queues(void) {
    i2cMutex = xSemaphoreCreateMutex();
    if (i2cMutex == NULL) {
        log_e("xSemaphoreCreateMutex");
        return false;
    }
    irq_queue = xQueueCreate(IRQ_QUEUELEN, sizeof(irqmsg_t));
    measurements_queue = new espidf::RingBuffer();
    measurements_queue->create(MEASMT_QUEUELEN, RINGBUF_TYPE_NOSPLIT);
    return true;
}

BaseType_t run_baro_task(void) {
#ifdef CONFIG_FREERTOS_UNICORE
    return xTaskCreate(run_baro_task, "run_baro_task", SOFTIRQ_STACKSIZE, NULL,
                       SOFTIRQ_PRIORITY, &baro_task);
#else
    return xTaskCreatePinnedToCore(run_baro_task, "run_baro_task", BAROTASK_STACKSIZE, NULL,
                                   BAROTASK_PRIORITY, &baro_task, BAROTASK_CORE);
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
void run_baro_task(void* arg) {
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
#define I2C_LOCK_WAIT 100
    // baro loop
    for (;;) {
        bool prs_measurement = (cycle++ & TEMP_COUNT_MASK);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if (xSemaphoreTake(i2cMutex, portTICK_PERIOD_MS * I2C_LOCK_WAIT) == pdTRUE) {
            int16_t ret;
            for (auto i = 0; i < num_dps_sensors; i++) {
                dps_sensors_t *dev = &dps_sensors[i];
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
        float timestamp = fseconds();
        vTaskDelay(250 / portTICK_PERIOD_MS);

        if (xSemaphoreTake(i2cMutex, portTICK_PERIOD_MS * I2C_LOCK_WAIT) == pdTRUE) {
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
                bs->timestamp = timestamp;
                if (prs_measurement) {
                    bs->type  = SAMPLE_PRESSURE;
                    bs->value = value / 100.0f;  // scale to hPa
                // } else {
                //     bs->type  = SAMPLE_TEMPERATURE;
                //     bs->value = value; // already degC
                }
                if (measurements_queue->send_complete(bs) != pdTRUE) {
                    commit_fail++;
                }
            }
            xSemaphoreGive(i2cMutex);
        }
        // i2cMutex released

        // dev->prs_conversion_time
        // release I2C lock during conversion



        // int16_t DpsClass::measurePressureOnce(float &result, uint8_t oversamplingRate)
        // {
        //     // start the measurement
        //     int16_t ret = startMeasurePressureOnce(oversamplingRate);
        //     if (ret != DPS__SUCCEEDED)
        //     {
        //         if (ret == DPS__FAIL_TOOBUSY)
        //         {
        //    		    standby();
        //    	    }
        //         return ret;
        //     }

        //     // wait until measurement is finished
        //     delay(calcBusyTime(0U, m_prsOsr) / DPS__BUSYTIME_SCALING);
        //     delay(DPS3xx__BUSYTIME_FAILSAFE);

        //     ret = getSingleResult(result);
        //     if (ret != DPS__SUCCEEDED)
        //     {
        //         standby();
        //     }
        //     return ret;
        // }


        // dps368_irq(static_cast<dps_sensors_t *>(msg.dev), msg.timestamp);
    }
#if 0
    for (;;) {
        while (xQueueReceive(irq_queue, &msg, 10 * portTICK_PERIOD_MS) == pdTRUE) {
            TOGGLE(TRIGGER2);

            devhdr_t *dh = static_cast<devhdr_t *>(msg.dev);
            i2c_gendev_t *gd = &dh->dev;
            gd->last_heard = msg.timestamp;

            switch (gd->type) {
                case DEV_DPS368: {
                        dps368_irq(static_cast<dps_sensors_t *>(msg.dev), msg.timestamp);
                    }
                    break;
#if defined(IMU_SUPPORT)

                case DEV_ICM_20948: {
                        icm20948_irq(static_cast<icm20948_t *>(msg.dev), msg.timestamp);
                    }
                    break;
#endif
                case DEV_M5STACK_IMU:
                    break;
                case DEV_NEO_M9N:
#ifdef UBLOX_SUPPORT
                    ublox_read(gd);
#endif
                    break;

                case DEV_MFRC522:
                    TOGGLE(TRIGGER2);
                    nfc_poll();
                    TOGGLE(TRIGGER2);
                    break;

                case DEV_BATTERY:
                    battery_check();
                    break;

                case DEV_MICROPHONE:
                    break;
            }
            if (uxQueueMessagesWaiting(irq_queue) == 0) {
                break;
            }
        }
        TOGGLE(TRIGGER2);

        yield();

        // check sensors being alive
        if (TIME_FOR(deadman)) {
            float now = fseconds();
            for (auto i = 0; i < num_dps_sensors; i++) {
                dps_sensors_t *d = &dps_sensors[i];
                if (!d->dev.device_present)
                    continue;

                // if (!d->dev.device_initialized)
                //     continue;
                if (now - d->dev.last_heard > i2c_timeout) {
                    if (d->dps_mode != DPS3XX_ERRORED)
                        continue;
                    log_e("%s timeout (%f sec) - reinit", d->dev.topic, i2c_timeout.get());
                    // might publish a sensor fault message here
                    int16_t ret = dps368_setup(i);
                    d->previous_alt = -1e6;
                    d->previous_time = -1e6;
                    d->initial_alt_values = INITIAL_ALTITUDE_VALUES;
                    d->dev.last_heard = now; // leave some time till next kick
                }
            }
#if defined(IMU_SUPPORT)

#ifdef USE_IRQ

            if (imu_sensor.dev.device_present &&
                    // !imu_sensor.dev.device_initialized &&
                    (now - imu_sensor.dev.last_heard > i2c_timeout.get())) {
                log_e("%s timeout (%f sec) - reinit", imu_sensor.dev.topic, i2c_timeout.get());
                imu_setup(&imu_sensor);
                imu_sensor.dev.last_heard = now; // leave some time till next kick
            }
#endif
#endif
            DONE_WITH(deadman);
        }

    }
#endif

}


