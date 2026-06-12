#include "ch422g_pwm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "waveshare_rgb_lcd_port.h"

// static const char *TAG = "ch422g_pwm";

// ── CH422G I²C config ────────────────────────────────────────────────
//#define CH422G_I2C_ADDR        0x24        // base address for OE/output regs
//#define CH422G_REG_OUT         0x46        // IO[7:0] output register address (write-only)
//#define I2C_MASTER_NUM         I2C_NUM_0   // match your board's I2C port
#define PWM_TARGET_FREQ_HZ     120
#define PWM_RESOLUTION_STEPS   100         // duty steps = 0..100 %

// ── Pin mask ─────────────────────────────────────────────────────────
// IO_EXPANDER_PIN_NUM_2 = bit 2
#define PWM_PIN_MASK           (1 << 2)

// ── Timing ───────────────────────────────────────────────────────────
// One PWM period = 1/120 Hz ≈ 8333 µs
// Split into RESOLUTION_STEPS slices → each slice ≈ 83 µs
#define PERIOD_US   (1000000 / PWM_TARGET_FREQ_HZ)          // 8333 µs
#define SLICE_US    (PERIOD_US / PWM_RESOLUTION_STEPS)      // 83 µs

static volatile uint8_t s_duty       = 0;    // 0–100
static volatile bool     s_running   = false;
static TaskHandle_t      s_task_handle = NULL;
static uint8_t           s_last_output = 0;  // shadow of current IO output byte

// ── Low-level CH422G write ────────────────────────────────────────────
static esp_err_t ch422g_write_output(uint8_t value)
{
    esp_err_t ret_value;
    uint8_t write_buf;
    if (value)
    {
        // Pull the backlight pin high to light the screen backlight
        write_buf = 0x1E;
        ret_value = i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    }
    else
    {        
        // Turn off the screen backlight by pulling the backlight pin low
        write_buf = 0x1A;
        ret_value = i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    }
    return ret_value;
}

static inline void pin_set(bool high)
{
    uint8_t next = high ? (s_last_output |  PWM_PIN_MASK)
                        : (s_last_output & ~PWM_PIN_MASK);
    if (next != s_last_output) {
        s_last_output = next;
        ch422g_write_output(next);
    }
}

// ── PWM task ─────────────────────────────────────────────────────────
static void pwm_task(void *arg)
{
    uint32_t step = 0;
    uint8_t write_buf;

    while (s_running) {
        uint8_t duty = s_duty;           // snapshot once per slice

        if (duty == 0) {
            // Turn off the screen backlight by pulling the backlight pin low
            write_buf = 0x1A;
            i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
            esp_rom_delay_us(PERIOD_US); // sleep a full period, skip counting
            continue;
        }
        if (duty >= 100) {
            // Pull the backlight pin high to light the screen backlight
            write_buf = 0x1E;
            i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
            esp_rom_delay_us(PERIOD_US);
            continue;
        }

        // Normal case: toggle based on step within period
        //pin_set(step < duty);
        step = (step + 1) % PWM_RESOLUTION_STEPS;
        if (step % 2)
        {
            // ESP_LOGI(TAG, "PWM ON");
            // Pull the backlight pin high to light the screen backlight
            write_buf = 0x1E;
            i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
        }
        else
        {
            // ESP_LOGI(TAG, "PWM OFF");
            // Turn off the screen backlight by pulling the backlight pin low
            write_buf = 0x1A;
            i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
        }
        //esp_rom_delay_us(SLICE_US);
        esp_rom_delay_us(PERIOD_US * 120);
    }

    pin_set(false);   // leave pin LOW on exit
    s_task_handle = NULL;
    vTaskDelete(NULL);
}

// ── Public API ────────────────────────────────────────────────────────
void ch422g_pwm_init(void)
{
    if (s_task_handle != NULL) return;   // already running

    s_running = true;
    s_duty    = 0;

    // Pin HIGH on core 1 to avoid interfering with LVGL on core 0
    xTaskCreatePinnedToCore(pwm_task, "ch422g_pwm",
                            2048, NULL,
                            3,  // higher priority
                            &s_task_handle, 1);

    ESP_LOGI(TAG, "SW-PWM started: %d Hz, %d steps, slice=%d µs",
             PWM_TARGET_FREQ_HZ, PWM_RESOLUTION_STEPS, SLICE_US);
}

void ch422g_pwm_set_duty(uint8_t duty_percent)
{
    s_duty = (duty_percent > 100) ? 100 : duty_percent;
}

void ch422g_pwm_stop(void)
{
    s_running = false;
    // task deletes itself; give it time to exit cleanly
    vTaskDelay(pdMS_TO_TICKS(20));
}