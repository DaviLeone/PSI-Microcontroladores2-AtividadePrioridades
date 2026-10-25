#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define LED_A_NODE DT_ALIAS(led0)
#define LED_B_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec ledA = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
static const struct gpio_dt_spec ledB = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);

#define PRIO_THREAD_A 5
#define PRIO_THREAD_B 7
#define TEMPO_A_MS    1500
#define TEMPO_B_MS    1000

void thread_A(void *p1, void *p2, void *p3)
{
    int64_t ini, fim;

    while (1) {
        ini = k_uptime_get();               /* tempo (ms) derivado do SysTick */

        gpio_pin_set_dt(&ledA, 1);
        for (volatile int i = 0; i < 200000; i++) {}
        gpio_pin_set_dt(&ledA, 0);

        fim = k_uptime_get();
        printk("[A] ini=%lld ms fim=%lld ms duracao=%lld ms\n", ini, fim, fim - ini);

        k_msleep(TEMPO_A_MS);
    }
}

void thread_B(void *p1, void *p2, void *p3)
{
    int64_t ini, fim;

    while (1) {
        ini = k_uptime_get();

        gpio_pin_set_dt(&ledB, 1);
        for (volatile int i = 0; i < 10000000; i++) {
            if (i % 100000 == 0) {
                /* apenas ocupa a CPU */
            }
        }
        gpio_pin_set_dt(&ledB, 0);

        fim = k_uptime_get();
        printk("[B] ini=%lld ms fim=%lld ms duracao=%lld ms\n", ini, fim, fim - ini);

        k_msleep(TEMPO_B_MS);
    }
}

K_THREAD_DEFINE(a_tid, 512, thread_A, NULL, NULL, NULL, PRIO_THREAD_A, 0, 0);
K_THREAD_DEFINE(b_tid, 512, thread_B, NULL, NULL, NULL, PRIO_THREAD_B, 0, 0);

int main(void)
{
    if (!device_is_ready(ledA.port) || !device_is_ready(ledB.port)) {
        return 0;
    }
    gpio_pin_configure_dt(&ledA, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledB, GPIO_OUTPUT_INACTIVE);

    printk("=== Demonstracao de Preempcao com LEDs ===\n");
    printk("Thread A (led0): prioridade %d\n", PRIO_THREAD_A);
    printk("Thread B (led2): prioridade %d\n", PRIO_THREAD_B);

    while (1) {
        k_sleep(K_FOREVER);
    }
    return 0;
}