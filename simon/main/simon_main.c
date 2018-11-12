#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#define LEDC_HS_CH0_GPIO  26
#define LEDC_HS_CH1_GPIO  25
#define LEDC_HS_CH2_GPIO  18
#define LEDC_HS_CH3_GPIO  19

#define LEDC_CH_NUM       4
#define LEDC_DUTY         4000
#define LEDC_FADE_TIME    50

#define BUTTON_PIN_0      13
#define BUTTON_PIN_1      14
#define BUTTON_PIN_2      4
#define BUTTON_PIN_3      5

#define NUM_BUTTONS       4
#define DEBOUNCE_TIME     10

// game defines...
#define MAX_LEVEL         10
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

QueueHandle_t q1;
gpio_num_t g_gpio;

//static char* tag = "simon_says";
int num_interrupts[LEDC_CH_NUM] = {0};
//static int blink_index = -1;
uint32_t gpio_intr_status;
ledc_channel_config_t ledc_channel[LEDC_CH_NUM];

void blink_led(int index)
{
    ledc_set_fade_with_time(ledc_channel[index].speed_mode,ledc_channel[index].channel, LEDC_DUTY, LEDC_FADE_TIME);
    ledc_fade_start(ledc_channel[index].speed_mode,ledc_channel[index].channel, LEDC_FADE_NO_WAIT);
    vTaskDelay(LEDC_FADE_TIME / portTICK_PERIOD_MS);

    ledc_set_fade_with_time(ledc_channel[index].speed_mode,ledc_channel[index].channel, 0, LEDC_FADE_TIME);
    ledc_fade_start(ledc_channel[index].speed_mode,ledc_channel[index].channel, LEDC_FADE_NO_WAIT);
    vTaskDelay(LEDC_FADE_TIME / portTICK_PERIOD_MS);
}

void IRAM_ATTR handle_button_0(void *args)
{
    gpio_intr_status = READ_PERI_REG(GPIO_STATUS_REG);
    gpio_num_t gpio = (gpio_num_t)BUTTON_PIN_0;
    ++num_interrupts[0];
    xQueueSendToBackFromISR(q1, &gpio,NULL);
}
void IRAM_ATTR handle_button_1(void *args)
{
    gpio_intr_status = READ_PERI_REG(GPIO_STATUS_REG);
    gpio_num_t gpio = (gpio_num_t)BUTTON_PIN_1;
    ++num_interrupts[1];
    xQueueSendToBackFromISR(q1, &gpio,NULL);
}
void IRAM_ATTR handle_button_2(void *args)
{
    gpio_intr_status = READ_PERI_REG(GPIO_STATUS_REG);
    gpio_num_t gpio = (gpio_num_t)BUTTON_PIN_2;
    ++num_interrupts[2];
    xQueueSendToBackFromISR(q1, &gpio,NULL);
}
void IRAM_ATTR handle_button_3(void *args)
{
    gpio_intr_status = READ_PERI_REG(GPIO_STATUS_REG);
    gpio_num_t gpio = (gpio_num_t)BUTTON_PIN_3;
    ++num_interrupts[3];
    xQueueSendToBackFromISR(q1, &gpio,NULL);
}

void init_gpio()
{
    q1 = xQueueCreate(10,sizeof(gpio_num_t));

    gpio_config_t config;
    config.pin_bit_mask = (1 << BUTTON_PIN_0) | (1 << BUTTON_PIN_1) | (1 << BUTTON_PIN_2) | (1 << BUTTON_PIN_3);
    config.mode         = GPIO_MODE_INPUT;
    config.pull_up_en   = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    config.intr_type    = GPIO_INTR_POSEDGE;
    gpio_config(&config);

    gpio_install_isr_service(0);

    gpio_isr_handler_add(BUTTON_PIN_0,handle_button_0,NULL);
    gpio_isr_handler_add(BUTTON_PIN_1,handle_button_1,NULL);
    gpio_isr_handler_add(BUTTON_PIN_2,handle_button_2,NULL);
    gpio_isr_handler_add(BUTTON_PIN_3,handle_button_3,NULL);
}

void init_leds()
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
        .freq_hz = 5000,                      // frequency of PWM signal
        .speed_mode = LEDC_HIGH_SPEED_MODE,   // timer mode
        .timer_num = LEDC_TIMER_0             // timer index
    };
    
    ledc_timer_config(&ledc_timer);

    ledc_channel[0].channel    = LEDC_CHANNEL_0;
    ledc_channel[0].duty       = 0;
    ledc_channel[0].gpio_num   = LEDC_HS_CH0_GPIO;
    ledc_channel[0].speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel[0].timer_sel  = LEDC_TIMER_0;

    ledc_channel[1].channel    = LEDC_CHANNEL_1;
    ledc_channel[1].duty       = 0;
    ledc_channel[1].gpio_num   = LEDC_HS_CH1_GPIO;
    ledc_channel[1].speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel[1].timer_sel  = LEDC_TIMER_0;

    ledc_channel[2].channel    = LEDC_CHANNEL_2;
    ledc_channel[2].duty       = 0;
    ledc_channel[2].gpio_num   = LEDC_HS_CH2_GPIO;
    ledc_channel[2].speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel[2].timer_sel  = LEDC_TIMER_0;

    ledc_channel[3].channel    = LEDC_CHANNEL_3;
    ledc_channel[3].duty       = 0;
    ledc_channel[3].gpio_num   = LEDC_HS_CH3_GPIO;
    ledc_channel[3].speed_mode = LEDC_HIGH_SPEED_MODE;
    ledc_channel[3].timer_sel  = LEDC_TIMER_0;

    for (int ch = 0; ch < LEDC_CH_NUM; ++ch)
        ledc_channel_config(&ledc_channel[ch]);

    ledc_fade_func_install(0);
}

typedef enum {
    NONE,
    SIMON_TURN,
    PLAYER_TURN
} TURN;
TURN turn = NONE;

void app_main()
{
    init_gpio();
    init_leds();

    int num_presses[NUM_BUTTONS] = {0};
    int v0 = 0,v1 = 0,v2 = 0,v3 = 0; // logic values of buttons
    int level = 0;
    int simon_pattern[MAX_LEVEL] = {0};
    int player_guess_level = 0;
    int button_index_pressed;

    //srand(time(NULL));

    for(;;) {

        switch(turn) {
            case NONE:
                // waiting for initiation
                if (v0)
                    turn = SIMON_TURN;
                break;
            case SIMON_TURN:

                vTaskDelay(1000/portTICK_PERIOD_MS);
                
                // choose a random value
                simon_pattern[level++] = esp_random() % 4;
                level = MIN(level,MAX_LEVEL);

                // show pattern
                for(int i = 0;i < level; ++i) {
                    
                    // blink the light
                    blink_led(simon_pattern[i]);
                    
                    // wait
                    vTaskDelay(1000 /portTICK_PERIOD_MS);
                }
                player_guess_level = 0;
                turn = PLAYER_TURN;
                break;
            case PLAYER_TURN:

                button_index_pressed = -1;

                if(v0 == 1)      button_index_pressed = 0;
                else if(v1 == 1) button_index_pressed = 1;
                else if(v2 == 1) button_index_pressed = 2;
                else if(v3 == 1) button_index_pressed = 3;

                if(button_index_pressed > -1) {
                    if(simon_pattern[player_guess_level] == button_index_pressed) {
                        ++player_guess_level;
                        if(player_guess_level == level) {
                            if (level == MAX_LEVEL) {
                                // won the game
                                // go bananas
                                for(int i = 0; i < 100; ++i)
                                    blink_led(esp_random() %4);

                                level = 0;
                                turn = NONE;
                            }
                            else {
                                // won the round
                                turn = SIMON_TURN;
                            }
                        }
                    }
                    else {
                        // loss the game
                        blink_led(3);
                        blink_led(3);
                        blink_led(3);
                        level = 0;
                        turn = NONE;
                    }
                }
                break;
        }

        BaseType_t r = xQueueReceive(q1, &g_gpio,(TickType_t)1);

        v0 = v1 = v2 = v3 = 0;
        if(r == pdTRUE) {

            v0 = gpio_get_level(BUTTON_PIN_0);
            v1 = gpio_get_level(BUTTON_PIN_1);
            v2 = gpio_get_level(BUTTON_PIN_2);
            v3 = gpio_get_level(BUTTON_PIN_3);


            if(v0 == 1) {++num_presses[0];blink_led(0);}
            if(v1 == 1) {++num_presses[1];blink_led(1);}
            if(v2 == 1) {++num_presses[2];blink_led(2);}
            if(v3 == 1) {++num_presses[3];blink_led(3);}

            if(v0 == 1 || v1 == 1 || v2 == 1 || v3 == 1)
                printf("%d,%d,%d,%d\n",num_presses[0],num_presses[1],num_presses[2],num_presses[3]);
        }

        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}
