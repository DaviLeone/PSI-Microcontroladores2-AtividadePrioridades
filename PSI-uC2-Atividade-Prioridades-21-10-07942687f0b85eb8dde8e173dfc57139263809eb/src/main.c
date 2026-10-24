#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>


#define LED_A_NODE DT_ALIAS(led0)  // LED verde
#define LED_B_NODE DT_ALIAS(led2)  // LED vermelho

static const struct gpio_dt_spec ledA = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
static const struct gpio_dt_spec ledB = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);


#define PRIO_THREAD_A 5 
#define PRIO_THREAD_B 7 
#define TEMPO_A_MS   1500   
#define TEMPO_B_MS   1000   


void thread_A(void *p1, void *p2, void *p3)
{
    uint64_t start_msA, end_msA;

    while (1) {
        start_msA = k_uptime_get();   

        gpio_pin_set_dt(&ledA, 1);  

        for (volatile int i = 0; i < 200000; i++) {}

        gpio_pin_set_dt(&ledA, 0);  

        end_msA = k_uptime_get();  

        printk("Tempo gasto no thread A: %llu ms\n", end_msA - start_msA);
        printk("Tempo de inicio e  de fim (respectivamente) do thread A: %llu %llu\n", start_msA, end_msA);
   
        k_msleep(TEMPO_A_MS); 


    }
}


void thread_B(void *p1, void *p2, void *p3)
{
    uint64_t start_msB, end_msB;

    while (1) {
        start_msB = k_uptime_get();  // tempo atual em ms  

        gpio_pin_set_dt(&ledB, 1);  // Liga LED vermelho

        // Simula processamento mais longo 
        for (volatile int i = 0; i < 10000000; i++) {
            if (i % 100000 == 0) {
                // Aqui não imprimimos nada — apenas ocupamos a CPU
            }
        }

        gpio_pin_set_dt(&ledB, 0);  // Desliga LED vermelho

        end_msB = k_uptime_get();  // tempo após o delay

        printk("Tempo decorrido para o thread B: %llu ms\n", end_msB - start_msB);
        printk("Tempo de inicio e fim (respectivamente) do thread B: %llu %llu\n", start_msB, end_msB);

        k_msleep(TEMPO_B_MS);       
    }
}


K_THREAD_DEFINE(a_tid, 512, thread_A, NULL, NULL, NULL,
                PRIO_THREAD_A, 0, 0);
K_THREAD_DEFINE(b_tid, 512, thread_B, NULL, NULL, NULL,
                PRIO_THREAD_B, 0, 0);

int main()
{
    if (!device_is_ready(ledA.port) || !device_is_ready(ledB.port)) {
        return 0;
    }
    gpio_pin_configure_dt(&ledA, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&ledB, GPIO_OUTPUT_INACTIVE);

    printk("=== Demonstração de Preempção com LEDs ===\n");
    printk("Thread A (vermelho): prioridade %d\n", PRIO_THREAD_A);
    printk("Thread B (verde): prioridade %d\n", PRIO_THREAD_B);

    while (1) {
        k_sleep(K_FOREVER);
    }

    return 0;
}
