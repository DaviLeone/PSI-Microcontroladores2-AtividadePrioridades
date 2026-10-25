#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define LED_A_NODE DT_ALIAS(led0)
#define LED_B_NODE DT_ALIAS(led2)

static const struct gpio_dt_spec ledA = GPIO_DT_SPEC_GET(LED_A_NODE, gpios);
static const struct gpio_dt_spec ledB = GPIO_DT_SPEC_GET(LED_B_NODE, gpios);

/*
 * Escolha o caso de teste (1..6) editando CASO abaixo.
 *  1: A=5 (sleep 1500) / B=7 (sleep 1000)   -> A preempta B (referencia)
 *  2: A=7 / B=5                              -> prioridades invertidas
 *  3: A=5 / B=5                              -> prioridades iguais
 *  4: A=5 (sleep 200) / B=7 (sleep 1000)     -> A acorda com muito mais frequencia
 *  5: A=7 / B=5 e B SEM k_sleep()            -> B monopoliza a CPU: A sofre starvation
 *  6: A=5 / B=-1 (cooperativa)               -> B nao pode ser preemptada por A
 */
#ifndef CASO
#define CASO 1
#endif

#if CASO == 1
  #define PRIO_THREAD_A 5
  #define PRIO_THREAD_B 7
  #define TEMPO_A_MS    1500
  #define TEMPO_B_MS    1000
#elif CASO == 2
  #define PRIO_THREAD_A 7
  #define PRIO_THREAD_B 5
  #define TEMPO_A_MS    1500
  #define TEMPO_B_MS    1000
#elif CASO == 3
  #define PRIO_THREAD_A 5
  #define PRIO_THREAD_B 5
  #define TEMPO_A_MS    1500
  #define TEMPO_B_MS    1000
#elif CASO == 4
  #define PRIO_THREAD_A 5
  #define PRIO_THREAD_B 7
  #define TEMPO_A_MS    200
  #define TEMPO_B_MS    1000
#elif CASO == 5
  #define PRIO_THREAD_A 7
  #define PRIO_THREAD_B 5
  #define TEMPO_A_MS    1500
  #define TEMPO_B_MS    0      /* 0 = thread B nao chama k_msleep() */
#elif CASO == 6
  #define PRIO_THREAD_A 5
  #define PRIO_THREAD_B (-1)   /* prioridade negativa = thread cooperativa */
  #define TEMPO_A_MS    1500
  #define TEMPO_B_MS    1000
#else
  #error "CASO invalido (use 1..6)"
#endif

/* Contadores de execucoes (usados pela main para detectar starvation) */
static volatile uint32_t contagem_A, contagem_B;

/* Custo (em ciclos) de uma chamada a k_cycle_get_32(), medido no boot */
static uint32_t overhead_ciclos;

static uint32_t calibra_overhead(void)
{
    uint32_t a = k_cycle_get_32();
    uint32_t b = k_cycle_get_32();
    return b - a;
}

/*
 * Para cada execucao medimos:
 *  - cpu   : tempo de CPU "puro" (ciclos -> us), sem contar o que foi preemptado
 *  - wall  : tempo de relogio (ms) entre inicio e fim; inclui preempcao
 *  - atraso: quanto a thread demorou para voltar a rodar depois do k_msleep()
 */
void thread_A(void *p1, void *p2, void *p3)
{
    int64_t ant_ms = 0, ini_ms, fim_ms, atraso_ms;
    bool primeira = true;

    while (1) {
        uint32_t ini_cyc = k_cycle_get_32();
        ini_ms = k_uptime_get();
        atraso_ms = primeira ? 0 : ini_ms - (ant_ms + TEMPO_A_MS);

        gpio_pin_set_dt(&ledA, 1);
        for (volatile int i = 0; i < 200000; i++) {}
        gpio_pin_set_dt(&ledA, 0);

        uint32_t cyc = k_cycle_get_32() - ini_cyc - overhead_ciclos;
        fim_ms = k_uptime_get();

        printk("[A] ini=%lld ms cpu=%u us wall=%lld ms atraso=%lld ms\n",
               ini_ms, k_cyc_to_us_floor32(cyc), fim_ms - ini_ms, atraso_ms);

        contagem_A++;
        primeira = false;
        ant_ms = k_uptime_get();
        k_msleep(TEMPO_A_MS);
    }
}

void thread_B(void *p1, void *p2, void *p3)
{
    int64_t ant_ms = 0, ini_ms, fim_ms, atraso_ms;
    bool primeira = true;

    while (1) {
        uint32_t ini_cyc = k_cycle_get_32();
        ini_ms = k_uptime_get();
        atraso_ms = primeira ? 0 : ini_ms - (ant_ms + TEMPO_B_MS);

        gpio_pin_set_dt(&ledB, 1);
        for (volatile int i = 0; i < 10000000; i++) {
            if (i % 100000 == 0) {
                /* apenas ocupa a CPU */
            }
        }
        gpio_pin_set_dt(&ledB, 0);

        uint32_t cyc = k_cycle_get_32() - ini_cyc - overhead_ciclos;
        fim_ms = k_uptime_get();

        printk("[B] ini=%lld ms cpu=%u us wall=%lld ms atraso=%lld ms\n",
               ini_ms, k_cyc_to_us_floor32(cyc), fim_ms - ini_ms, atraso_ms);

        contagem_B++;
        primeira = false;
        ant_ms = k_uptime_get();
#if TEMPO_B_MS > 0
        k_msleep(TEMPO_B_MS);
#endif
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

    overhead_ciclos = calibra_overhead();

    uint32_t hz = (uint32_t)sys_clock_hw_cycles_per_sec();

    printk("=== Demonstracao de Preempcao com LEDs (CASO %d) ===\n", CASO);
    printk("Clock do sistema: %u Hz (1 ciclo = %u ns)\n", hz, 1000000000U / hz);
    printk("Tick do kernel: %d Hz | overhead k_cycle_get_32: %u ciclos\n",
           CONFIG_SYS_CLOCK_TICKS_PER_SEC, overhead_ciclos);
    printk("Thread A (led0): prioridade %d, sleep %d ms\n", PRIO_THREAD_A, TEMPO_A_MS);
    printk("Thread B (led2): prioridade %d, sleep %d ms\n", PRIO_THREAD_B, TEMPO_B_MS);

    /* A main tem prioridade 0 (maior que A e B): serve de "batimento cardiaco" */
    while (1) {
        k_sleep(K_SECONDS(1));
        printk("[main] t=%lld ms | execucoes: A=%u B=%u\n",
               k_uptime_get(), contagem_A, contagem_B);
    }
    return 0;
}