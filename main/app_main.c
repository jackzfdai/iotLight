/* 
   Iot Synchronized Lights

   Author: ZF D

   Based on the MQTT (over TCP) Example provided in ESP-IDF

   This code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "tcpip_adapter.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "driver/ledc.h"
#include "esp_err.h"
#include "driver/timer.h"

#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_LS_TIMER          LEDC_TIMER_1
#define LEDC_LS_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_TEST_DUTY         (255)
#define LEDC_TEST_FADE_TIME    (3000)

#define PRIMARY_COLOR_CNT 3
#define BLUE_INDEX 0
#define GREEN_INDEX 1
#define RED_INDEX 2

#define LEDC_BLUE_GPIO       (18)
#define LEDC_BLUE_CHANNEL    LEDC_CHANNEL_0
#define LEDC_GREEN_GPIO       (17)
#define LEDC_GREEN_CHANNEL    LEDC_CHANNEL_1
#define LEDC_RED_GPIO       (19)
#define LEDC_RED_CHANNEL    LEDC_CHANNEL_2

#define LIGHT_A
#ifndef LIGHT_A
#define LIGHT_B
#endif

typedef enum {
    LIGHT_EVT_CW,
    LIGHT_EVT_CCW,
    LIGHT_EVT_ON,
    LIGHT_EVT_OFF,
    LIGHT_EVT_FLASH,
    LIGHT_EVT_STOP_FLASH,
    LIGHT_EVT_EXTERNAL,
    LIGHT_EVT_MAX
} light_evt_t; 

typedef enum {
    COLOR_BLUE,
    COLOR_GREEN,
    COLOR_RED,
    COLOR_MAX
} color_t; 

typedef enum {
    TOPIC_RED,
    TOPIC_BLUE,
    TOPIC_GREEN,
    TOPIC_ON,
    TOPIC_FLASH,
    TOPIC_MAX
} topic_t; 

static const char *TAG = "MQTT_EXAMPLE";

static esp_mqtt_client_handle_t mqtt_client = NULL; 

static volatile uint8_t color [PRIMARY_COLOR_CNT]; 

QueueHandle_t light_evt_queue = NULL; 

static volatile BaseType_t lights_on = pdFALSE; 
static volatile BaseType_t flash = pdFALSE; 

#ifdef LIGHT_A
static const char red_topic_pub [19] = "/topic/red_light_a"; 
static const char blue_topic_pub [21] = "/topic/blue_light_a"; 
static const char green_topic_pub [22] = "/topic/green_light_a"; 
static const char on_topic_pub [22] = "/topic/on_light_a"; 
static const char flash_topic_pub [22] = "/topic/flash_light_a"; 
static const char red_topic_sub [19] = "/topic/red_light_b"; 
static const char blue_topic_sub [21] = "/topic/blue_light_b"; 
static const char green_topic_sub [22] = "/topic/green_light_B"; 
static const char on_topic_sub [22] = "/topic/on_light_b"; 
static const char flash_topic_sub [22] = "/topic/flash_light_b"; 
#endif

#ifdef LIGHT_B
static const char red_topic_pub [19] = "/topic/red_light_b"; 
static const char blue_topic_pub [21] = "/topic/blue_light_b"; 
static const char green_topic_pub [22] = "/topic/green_light_b"; 
static const char on_topic_pub [22] = "/topic/on_light_b"; 
static const char flash_topic_pub [22] = "/topic/flash_light_b"; 
static const char red_topic_sub [19] = "/topic/red_light_a"; 
static const char blue_topic_sub [21] = "/topic/blue_light_a"; 
static const char green_topic_sub [22] = "/topic/green_light_a"; 
static const char on_topic_sub [22] = "/topic/on_light_a"; 
static const char flash_topic_sub [22] = "/topic/flash_light_a"; 
#endif

static BaseType_t cmp_topic_sub (char * evt_topic, size_t len, topic_t topic)
{

    const char * topic_str = NULL; 

    switch (topic)
    {
        case TOPIC_RED:
            topic_str = red_topic_sub; 
        break;
        case TOPIC_BLUE:
            topic_str = blue_topic_sub; 
        break; 
        case TOPIC_GREEN:
            topic_str = green_topic_sub; 
        break;
        case TOPIC_ON:
            topic_str = on_topic_sub; 
        break;
        case TOPIC_FLASH:
            topic_str = flash_topic_sub; 
        break; 
        default:
        break;
    }

    BaseType_t ret = pdTRUE; 
    for (uint32_t i = 0; i < len && topic_str[i] != 0; i ++)
    {
        if (topic_str[i] != evt_topic[i])
        {
            ret = pdFALSE; 
        }
    }
    return ret; 
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            // msg_id = esp_mqtt_client_publish(client, "/topic/red", "data_3", 0, 1, 0);
            // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, red_topic_pub, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, blue_topic_pub, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, green_topic_pub, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, on_topic_pub, 2);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, flash_topic_pub, 2);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, red_topic_sub, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, blue_topic_sub, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, green_topic_sub, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, on_topic_sub, 2);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, flash_topic_sub, 2);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            // msg_id = esp_mqtt_client_publish(client, "/topic/red", "data", 0, 0, 0);
            // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);

            // ESP_LOGI(TAG, "Topic len %d, red static size %d", event->topic_len, sizeof(*red_topic)); 

            if (event->data_len == 1)
            {
                BaseType_t update = pdFALSE; 
                if (cmp_topic_sub(event->topic, event->topic_len, TOPIC_RED))
                {
                    ESP_LOGI(TAG, "Rx'ed red topic, data %d", (uint8_t) *(event->data)); 
                    color[RED_INDEX] = (uint8_t) *(event->data); 
                    update = pdTRUE; 
                    // if (color[RED_INDEX] != (uint8_t) *(event->data))
                    // {
                    //     color[RED_INDEX] = (uint8_t) *(event->data); 
                    //     update = pdTRUE; 
                    // }
                }
                else if (cmp_topic_sub(event->topic, event->topic_len, TOPIC_BLUE))
                {
                    ESP_LOGI(TAG, "Rx'ed blue topic, data %d", (uint8_t) *(event->data)); 
                    color[BLUE_INDEX] = (uint8_t) *(event->data); 
                    update = pdTRUE; 
                    // if (color[BLUE_INDEX] != (uint8_t) *(event->data))
                    // {
                    //     color[BLUE_INDEX] = (uint8_t) *(event->data); 
                    //     update = pdTRUE; 
                    // }
                }
                else if (cmp_topic_sub(event->topic, event->topic_len, TOPIC_GREEN))
                {
                    ESP_LOGI(TAG, "Rx'ed green topic, data %d", (uint8_t) *(event->data)); 
                    color[GREEN_INDEX] = (uint8_t) *(event->data); 
                    update = pdTRUE; 
                    // if (color[GREEN_INDEX] != (uint8_t) *(event->data))
                    // {
                    //     color[GREEN_INDEX] = (uint8_t) *(event->data); 
                    //     update = pdTRUE; 
                    // }
                }
                else if (cmp_topic_sub(event->topic, event->topic_len, TOPIC_ON))
                {
                    ESP_LOGI(TAG, "Rx'ed lights on topic, data %d", (uint8_t) *(event->data)); 
                    if ((uint8_t) *(event->data) == 1)
                    {
                        lights_on = pdTRUE; 
                        update = pdTRUE; 
                    } 
                    else if ((uint8_t) *(event->data) == 0)
                    {
                        lights_on = pdFALSE; 
                        update = pdTRUE; 
                    }
                    // if ((uint8_t) *(event->data) == 1 && lights_on == pdFALSE)
                    // {
                    //     lights_on = pdTRUE; 
                    //     update = pdTRUE; 
                    // } 
                    // else if ((uint8_t) *(event->data) == 0 && lights_on == pdTRUE)
                    // {
                    //     lights_on = pdFALSE; 
                    //     update = pdTRUE; 
                    // }
                }
                else if (cmp_topic_sub(event->topic, event->topic_len, TOPIC_FLASH))
                {
                    if ((uint8_t) *(event->data) == 1)
                    {
                        flash = pdTRUE; 
                        update = pdTRUE; 
                    } 
                    else if ((uint8_t) *(event->data) == 0)
                    {
                        flash = pdFALSE; 
                        update = pdTRUE; 
                    }
                    // if ((uint8_t) *(event->data) == 1 && flash == pdFALSE)
                    // {
                    //     flash = pdTRUE; 
                    //     update = pdTRUE; 
                    // } 
                    // else if ((uint8_t) *(event->data) == 0 && flash == pdTRUE)
                    // {
                    //     flash = pdFALSE; 
                    //     update = pdTRUE; 
                    // }
                }

                if (update == pdTRUE)
                {
                    uint8_t light_evt = LIGHT_EVT_EXTERNAL; 
                    xQueueSend(light_evt_queue, &light_evt, 0);
                }  
            }

            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    configASSERT(mqtt_client); 
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);
    esp_mqtt_client_start(mqtt_client);
}

// static void test_task(void * pvParam) 
// {
//     static uint8_t red = 0; 
//     while(1) {
//         red = (red + 1) % 0xFF; 
        
//         vTaskDelay(pdMS_TO_TICKS(1000)); 
//     }
// }

ledc_channel_config_t ledc_channel[PRIMARY_COLOR_CNT]; 

static void ledc_setup (void)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT, // resolution of PWM duty
        .freq_hz = 8000,                      // frequency of PWM signal
        .speed_mode = LEDC_HS_MODE,           // timer mode
        .timer_num = LEDC_HS_TIMER,            // timer index
        .clk_cfg = LEDC_AUTO_CLK,              // Auto select the source clock
    };
    ledc_timer_config(&ledc_timer); 

    ledc_channel[BLUE_INDEX].channel    = LEDC_BLUE_CHANNEL;
    ledc_channel[BLUE_INDEX].duty       = 0;
    ledc_channel[BLUE_INDEX].gpio_num   = LEDC_BLUE_GPIO;
    ledc_channel[BLUE_INDEX].speed_mode = LEDC_HS_MODE;
    ledc_channel[BLUE_INDEX].hpoint     = 0;
    ledc_channel[BLUE_INDEX].timer_sel  = LEDC_HS_TIMER;

    ledc_channel_config(&ledc_channel[BLUE_INDEX]);

    ledc_channel[GREEN_INDEX].channel    = LEDC_GREEN_CHANNEL;
    ledc_channel[GREEN_INDEX].duty       = 0;
    ledc_channel[GREEN_INDEX].gpio_num   = LEDC_GREEN_GPIO;
    ledc_channel[GREEN_INDEX].speed_mode = LEDC_HS_MODE;
    ledc_channel[GREEN_INDEX].hpoint     = 0;
    ledc_channel[GREEN_INDEX].timer_sel  = LEDC_HS_TIMER;

    ledc_channel_config(&ledc_channel[GREEN_INDEX]);

    ledc_channel[RED_INDEX].channel    = LEDC_RED_CHANNEL;
    ledc_channel[RED_INDEX].duty       = 0;
    ledc_channel[RED_INDEX].gpio_num   = LEDC_RED_GPIO;
    ledc_channel[RED_INDEX].speed_mode = LEDC_HS_MODE;
    ledc_channel[RED_INDEX].hpoint     = 0;
    ledc_channel[RED_INDEX].timer_sel  = LEDC_HS_TIMER;

    ledc_channel_config(&ledc_channel[RED_INDEX]);

    ledc_fade_func_install(0); 
}

// static void fade_test(void * param)
// {
//     while (1) {
//     printf("1. LEDC fade up to duty = %d\n", LEDC_TEST_DUTY);
//     ledc_set_fade_with_time(ledc_channel.speed_mode,
//             ledc_channel.channel, LEDC_TEST_DUTY, LEDC_TEST_FADE_TIME);
//     ledc_fade_start(ledc_channel.speed_mode,
//             ledc_channel.channel, LEDC_FADE_NO_WAIT);
//     vTaskDelay(LEDC_TEST_FADE_TIME / portTICK_PERIOD_MS);

//     printf("2. LEDC fade down to duty = 0\n");
//     ledc_set_fade_with_time(ledc_channel.speed_mode,
//             ledc_channel.channel, 0, LEDC_TEST_FADE_TIME);
//     ledc_fade_start(ledc_channel.speed_mode,
//             ledc_channel.channel, LEDC_FADE_NO_WAIT);
//     vTaskDelay(LEDC_TEST_FADE_TIME / portTICK_PERIOD_MS);

//     printf("3. LEDC set duty = %d without fade\n", LEDC_TEST_DUTY);
//     ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, LEDC_TEST_DUTY);
//     ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);
//     vTaskDelay(1000 / portTICK_PERIOD_MS);

//     printf("4. LEDC set duty = 0 without fade\n");
//     ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, 0);
//     ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);

//     vTaskDelay(1000 / portTICK_PERIOD_MS);
//     }
// }

#define FADE_TIME_MS 10 

static void set_color (color_t color, uint8_t duty)
{
    switch (color)
    {
        case COLOR_BLUE:
            ledc_set_fade_with_time(ledc_channel[BLUE_INDEX].speed_mode,
            ledc_channel[BLUE_INDEX].channel, duty, FADE_TIME_MS);
            ledc_fade_start(ledc_channel[BLUE_INDEX].speed_mode,
            ledc_channel[BLUE_INDEX].channel, LEDC_FADE_NO_WAIT);
        break;
        case COLOR_GREEN:
            ledc_set_fade_with_time(ledc_channel[GREEN_INDEX].speed_mode,
            ledc_channel[GREEN_INDEX].channel, duty, FADE_TIME_MS);
            ledc_fade_start(ledc_channel[GREEN_INDEX].speed_mode,
            ledc_channel[GREEN_INDEX].channel, LEDC_FADE_NO_WAIT);
        break;
        case COLOR_RED:
            ledc_set_fade_with_time(ledc_channel[RED_INDEX].speed_mode,
            ledc_channel[RED_INDEX].channel, duty, FADE_TIME_MS);
            ledc_fade_start(ledc_channel[RED_INDEX].speed_mode,
            ledc_channel[RED_INDEX].channel, LEDC_FADE_NO_WAIT);
        break; 
        default:
        break; 
    }

    vTaskDelay(FADE_TIME_MS / portTICK_PERIOD_MS);
}

static void mqtt_update_color(color_t color, uint8_t duty)
{
    int msg_id; 

    static uint8_t last_red = 0; 
    static uint8_t last_blue = 0; 
    static uint8_t last_green = 0; 

    switch (color)
    {
        case COLOR_BLUE:
            if (duty != last_blue)
            {
                msg_id = esp_mqtt_client_publish(mqtt_client, blue_topic_pub, (char *) &duty, 1, 1, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                last_blue = duty; 
            }
        break;
        case COLOR_GREEN:
            if (duty != last_green)
            {
                msg_id = esp_mqtt_client_publish(mqtt_client, green_topic_pub, (char *) &duty, 1, 1, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                last_green = duty; 
            }
        break;
        case COLOR_RED:
            if (duty != last_red)
            {
                msg_id = esp_mqtt_client_publish(mqtt_client, red_topic_pub, (char *) &duty, 1, 1, 0);
                ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
                last_red = duty; 
            }
        break; 
        default:
        break; 
    }
    
}

static void mqtt_update_flash (BaseType_t flash_state)
{
    // static uint8_t last_on = 0;
    // static BaseType_t last_flash = 0; 
    
    // if (flash_state != )
    if (flash_state == pdTRUE)
    {
        uint8_t state = 1; 
        int msg_id = esp_mqtt_client_publish(mqtt_client, flash_topic_pub, (char *) &state, 1, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    }
    else if (flash_state == pdFALSE)
    {
        uint8_t state = 0; 
        int msg_id = esp_mqtt_client_publish(mqtt_client, flash_topic_pub, (char *) &state, 1, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    }
}

static void mqtt_update_on (BaseType_t on_state)
{
    if (on_state == pdTRUE)
    {
        uint8_t state = 1; 
        int msg_id = esp_mqtt_client_publish(mqtt_client, on_topic_pub, (char *) &state, 1, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    }
    else if (on_state == pdFALSE)
    {
        uint8_t state = 0; 
        int msg_id = esp_mqtt_client_publish(mqtt_client, on_topic_pub, (char *) &state, 1, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    }
}

static volatile BaseType_t enc_hit = pdFALSE; 
static volatile BaseType_t but_hit = pdFALSE; 

#define ENC_A_PIN GPIO_NUM_27
#define ENC_B_PIN GPIO_NUM_23 
#define ENC_SW_PIN GPIO_NUM_14

SemaphoreHandle_t enc_but_update = NULL; 
QueueHandle_t enc_queue = NULL;
QueueHandle_t but_queue = NULL;

TaskHandle_t enc_debounce_task = NULL; 
TaskHandle_t enc_but_update_task_handle = NULL; 

TaskHandle_t but_debounce_task = NULL; 

static uint8_t enc_but_msg = 1; 

static void encoder_isr(void * param)
{
    BaseType_t yield = pdFALSE; 
    BaseType_t ret = pdFALSE; 

    if (enc_hit == pdFALSE)
    {
        ret = xQueueSendFromISR(enc_queue, &enc_but_msg, &yield); 
        enc_hit = pdTRUE; 
    }
    
    if (yield == pdTRUE && ret == pdTRUE)
    {
        portYIELD_FROM_ISR(); 
    }
}

static void IRAM_ATTR encoder_debounced_handler(void * param)
{
    uint32_t timer_intr = timer_group_intr_get_in_isr(TIMER_GROUP_1);

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if (timer_intr & TIMER_INTR_T0) {
        // evt.type = TEST_WITHOUT_RELOAD;
        timer_group_intr_clr_in_isr(TIMER_GROUP_1, TIMER_0);
    }

    BaseType_t yield = pdFALSE; 
    xSemaphoreGiveFromISR(enc_but_update, &yield); 
    enc_hit = pdFALSE; 
}

static void encoder_debounce_task (void * param)
{
    while (1)
    {
        uint8_t enc_msg_rx = 0; 
        xQueueReceive(enc_queue, &enc_msg_rx, portMAX_DELAY); 
        if (enc_msg_rx == 1)
        {
            timer_set_counter_value(TIMER_GROUP_1, TIMER_0, 5); 
            timer_set_alarm_value(TIMER_GROUP_1, TIMER_0, 0); 
            timer_set_alarm(TIMER_GROUP_1, TIMER_0, TIMER_ALARM_EN); 
            timer_start(TIMER_GROUP_1, TIMER_0); 
            // ESP_LOGI(TAG, "Timer init"); 
        }
    }
}

static uint32_t CW_cnt = 0; 
static uint32_t CCW_cnt = 0; 

#define HOLD_TICK 5000 / portTICK_PERIOD_MS

static volatile BaseType_t hold_on = pdFALSE;

static void enc_but_update_task (void * param)
{
    uint8_t existing_A = gpio_get_level(ENC_A_PIN);
    uint8_t existing_B = gpio_get_level(ENC_B_PIN); 
    uint8_t existing_SW = gpio_get_level(ENC_SW_PIN); 

    // static TickType_t hold_start_time = 0;  

    while (1) 
    {
        xSemaphoreTake(enc_but_update, portMAX_DELAY);

        uint8_t A_lvl = gpio_get_level(ENC_A_PIN);
        uint8_t B_lvl = gpio_get_level(ENC_B_PIN); 
        uint8_t SW_lvl = gpio_get_level(ENC_SW_PIN); 

        if (existing_A == 1 && existing_B == 1)
        {
            if (A_lvl == 1 && B_lvl == 0)
            {
                CCW_cnt ++; 
                ESP_LOGI(TAG, "CCW");
                uint8_t light_evt = LIGHT_EVT_CCW; 
                xQueueSend(light_evt_queue, &light_evt, 0); 
            }
            else if (A_lvl == 0 && B_lvl == 1)
            {
                CW_cnt ++; 
                ESP_LOGI(TAG, "CW"); 
                uint8_t light_evt = LIGHT_EVT_CW; 
                xQueueSend(light_evt_queue, &light_evt, 0); 
            }
        }

        existing_A = A_lvl;
        existing_B = B_lvl; 

        if (SW_lvl != existing_SW)
        {
            ESP_LOGI(TAG, "SW state change"); 
            if (SW_lvl == 1)
            {
                uint8_t light_evt = LIGHT_EVT_MAX; 
                // TickType_t curr_tick = xTaskGetTickCount(); 
                // uint64_t timer_counter_val = 5000*30; 
                // timer_get_counter_value(TIMER_GROUP_0, TIMER_1, &timer_counter_val);
                if (flash == pdTRUE)
                {
                    if (hold_on == pdTRUE)
                    {
                        flash = pdFALSE; 
                        light_evt = LIGHT_EVT_FLASH; 
                        ESP_LOGI(TAG, "End flash"); 
                    }
                    
                }
                else if (lights_on == pdFALSE)
                {
                    light_evt = LIGHT_EVT_ON; 
                    lights_on = pdTRUE;
                }
                else 
                {
                    light_evt = LIGHT_EVT_OFF; 
                    lights_on = pdFALSE; 
                }
                xQueueSend(light_evt_queue, &light_evt, 0);
                timer_pause(TIMER_GROUP_0, TIMER_1); 
                hold_on = pdFALSE; 
            }
            else if (SW_lvl == 0)
            {
                if (hold_on == pdFALSE)
                {
                    hold_on = pdTRUE; 
                    timer_set_counter_value(TIMER_GROUP_0, TIMER_1, 5000*3); 
                    timer_set_alarm_value(TIMER_GROUP_0, TIMER_1, 0); 
                    timer_set_alarm(TIMER_GROUP_0, TIMER_1, TIMER_ALARM_EN); 
                    timer_start(TIMER_GROUP_0, TIMER_1); 
                }
            }
        }
        existing_SW = SW_lvl; 
    }
}

static void IRAM_ATTR button_hold_timeout (void * param)
{
    uint32_t timer_intr = timer_group_intr_get_in_isr(TIMER_GROUP_0);

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if (timer_intr & TIMER_INTR_T1) {
        // evt.type = TEST_WITHOUT_RELOAD;
        timer_group_intr_clr_in_isr(TIMER_GROUP_0, TIMER_1);
    }

    flash = pdTRUE; 
    uint8_t light_evt = LIGHT_EVT_FLASH; 
    xQueueSendFromISR(light_evt_queue, &light_evt, 0);
    hold_on = pdFALSE; 
}

static void button_isr (void * param)
{
    BaseType_t yield = pdFALSE; 
    BaseType_t ret = pdFALSE; 

    if (but_hit == pdFALSE)
    {
        ret = xQueueSendFromISR(but_queue, &enc_but_msg, &yield); 
        but_hit = pdTRUE; 
    }
    
    if (yield == pdTRUE && ret == pdTRUE)
    {
        portYIELD_FROM_ISR(); 
    }
}

static void IRAM_ATTR button_debounced_handler (void * param)
{
    uint32_t timer_intr = timer_group_intr_get_in_isr(TIMER_GROUP_1);

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if (timer_intr & TIMER_INTR_T1) {
        // evt.type = TEST_WITHOUT_RELOAD;
        timer_group_intr_clr_in_isr(TIMER_GROUP_1, TIMER_1);
    }

    BaseType_t yield = pdFALSE; 
    xSemaphoreGiveFromISR(enc_but_update, &yield); 
    but_hit = pdFALSE; 
}

static void button_debounce_task (void * param)
{
    while (1)
    {
        uint8_t but_msg_rx = 0; 
        xQueueReceive(but_queue, &but_msg_rx, portMAX_DELAY); 
        if (but_msg_rx == 1)
        {
            timer_set_counter_value(TIMER_GROUP_1, TIMER_1, 50); 
            timer_set_alarm_value(TIMER_GROUP_1, TIMER_1, 0); 
            timer_set_alarm(TIMER_GROUP_1, TIMER_1, TIMER_ALARM_EN); 
            timer_start(TIMER_GROUP_1, TIMER_1); 
        }
    }
}

static void encoder_setup(void)
{
    timer_config_t timer_config; 
    timer_config.divider = 16000;
    timer_config.counter_dir = TIMER_COUNT_DOWN; 
    timer_config.intr_type = TIMER_INTR_LEVEL; 
    timer_config.counter_en = TIMER_PAUSE; 
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_1, TIMER_0, &timer_config));    
    timer_enable_intr(TIMER_GROUP_1, TIMER_0); 
    timer_isr_register(TIMER_GROUP_1, TIMER_0, &encoder_debounced_handler, NULL, ESP_INTR_FLAG_IRAM, NULL); 

    timer_config_t button_timer_config; 
    button_timer_config.divider = 16000;
    button_timer_config.counter_dir = TIMER_COUNT_DOWN; 
    button_timer_config.intr_type = TIMER_INTR_LEVEL; 
    button_timer_config.counter_en = TIMER_PAUSE; 
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_1, TIMER_1, &button_timer_config));    
    timer_enable_intr(TIMER_GROUP_1, TIMER_1); 
    timer_isr_register(TIMER_GROUP_1, TIMER_1, &button_debounced_handler, NULL, ESP_INTR_FLAG_IRAM, NULL); 
    
    timer_config_t button_hold_timer_config; 
    button_hold_timer_config.divider = 16000;
    button_hold_timer_config.counter_dir = TIMER_COUNT_DOWN; 
    button_hold_timer_config.intr_type = TIMER_INTR_LEVEL; 
    button_hold_timer_config.counter_en = TIMER_PAUSE; 
    ESP_ERROR_CHECK(timer_init(TIMER_GROUP_0, TIMER_1, &button_hold_timer_config));    
    timer_enable_intr(TIMER_GROUP_0, TIMER_1); 
    timer_isr_register(TIMER_GROUP_0, TIMER_1, &button_hold_timeout, NULL, ESP_INTR_FLAG_IRAM, NULL); 

    ESP_ERROR_CHECK(gpio_pullup_en(ENC_A_PIN));
    ESP_ERROR_CHECK(gpio_pullup_en(ENC_B_PIN));
    ESP_ERROR_CHECK(gpio_pullup_en(ENC_SW_PIN));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    ESP_ERROR_CHECK(gpio_isr_handler_add(ENC_A_PIN, &encoder_isr, NULL)); 
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENC_B_PIN, &encoder_isr, NULL)); 
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENC_SW_PIN, &button_isr, NULL)); 

    ESP_ERROR_CHECK(gpio_intr_enable(ENC_A_PIN));
    ESP_ERROR_CHECK(gpio_set_intr_type(ENC_A_PIN, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_intr_enable(ENC_B_PIN));
    ESP_ERROR_CHECK(gpio_set_intr_type(ENC_B_PIN, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_intr_enable(ENC_SW_PIN));
    ESP_ERROR_CHECK(gpio_set_intr_type(ENC_SW_PIN, GPIO_INTR_ANYEDGE));

    enc_but_update = xSemaphoreCreateCounting(2, 0); 
    configASSERT(enc_but_update); 

    // button_hit = xSemaphoreCreateBinary(); 
    // configASSERT(button_hit); 

    enc_queue = xQueueCreate(1, sizeof(uint8_t));
    configASSERT(enc_queue); 

    but_queue = xQueueCreate(1, sizeof(uint8_t));
    configASSERT(but_queue); 

    xTaskCreate(encoder_debounce_task, "Enc Debounce", 2048, NULL, 16, &enc_debounce_task); 
    configASSERT(enc_debounce_task); 

    xTaskCreate(button_debounce_task, "But Debounce", 2048, NULL, 16, &but_debounce_task); 
    configASSERT(but_debounce_task); 

    xTaskCreate(enc_but_update_task, "Enc But Update", 2048, NULL, 16, &enc_but_update_task_handle);
    configASSERT(enc_but_update_task_handle); 
}

static volatile uint8_t red = 0; 
static volatile uint8_t blue = 255; 
static volatile uint8_t green = 0; 

#define BLUE_INDEX 0
#define GREEN_INDEX 1
#define RED_INDEX 2
#define COLOR_INCR 10

#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

static void color_update_task(void * param)
{
    color[0] = 255; 
    color[1] = 0;
    color[2] = 0; 

    uint8_t ccw_index = 0;
    uint8_t cw_index = 1; 

    // set_color(COLOR_BLUE, color[BLUE_INDEX]); 
    // set_color(COLOR_GREEN, color[GREEN_INDEX]);
    // set_color(COLOR_RED, color[RED_INDEX]); 

    while (1)
    {
        // ESP_LOGI(TAG, "CW %d, CCW %d", CW_cnt, CCW_cnt); 

        // uint8_t A_lvl = gpio_get_level(ENC_A_PIN);
        // uint8_t B_lvl = gpio_get_level(ENC_B_PIN); 
        // uint8_t SW_lvl = gpio_get_level(ENC_SW_PIN); 
        // ESP_LOGI(TAG, "A %d | B %d | SW %d", A_lvl, B_lvl, SW_lvl); 

        // vTaskDelay(1000/portTICK_PERIOD_MS); 

        uint8_t evt = LIGHT_EVT_MAX; 
        xQueueReceive(light_evt_queue, &evt, portMAX_DELAY); 

        switch (evt) {
            case LIGHT_EVT_CW:
                if (color[ccw_index] == 0 && color[cw_index] == 255)
                {
                    ccw_index = (ccw_index + 1) % PRIMARY_COLOR_CNT; 
                    cw_index = (ccw_index + 1) % PRIMARY_COLOR_CNT;
                }
                color[ccw_index] = color[ccw_index] - min(COLOR_INCR, color[ccw_index]); 
                color[cw_index] = color[cw_index] + min(COLOR_INCR, 255 - color[cw_index]); 
            break;
            case LIGHT_EVT_CCW:
                if (color[ccw_index] == 255 && color[cw_index] == 0)
                {
                    ccw_index = (ccw_index - 1 + PRIMARY_COLOR_CNT) % PRIMARY_COLOR_CNT; 
                    cw_index = (ccw_index + 1) % PRIMARY_COLOR_CNT;
                }
                color[ccw_index] = color[ccw_index] + min(COLOR_INCR, 255 - color[ccw_index]); 
                color[cw_index] = color[cw_index] - min(COLOR_INCR, color[cw_index]); 
            break;
            default:
            break; 
        }

        ESP_LOGI(TAG, "B %d | G %d | R %d | On %d", color[BLUE_INDEX], color[GREEN_INDEX], color[RED_INDEX], lights_on); 

        if (evt != LIGHT_EVT_EXTERNAL)
        {
            if (evt == LIGHT_EVT_CW || evt == LIGHT_EVT_CCW)
            {
                mqtt_update_color(COLOR_BLUE, color[BLUE_INDEX]); 
                mqtt_update_color(COLOR_GREEN, color[GREEN_INDEX]);
                mqtt_update_color(COLOR_RED, color[RED_INDEX]);
            }

            if (evt == LIGHT_EVT_ON || evt == LIGHT_EVT_OFF)
            {
                mqtt_update_on(lights_on); 
            }

            if (evt == LIGHT_EVT_FLASH)
            {
                mqtt_update_flash(flash); 
            }
        }

        if (lights_on == pdTRUE) 
        {
            if (flash == pdFALSE) 
            {
                set_color(COLOR_BLUE, color[BLUE_INDEX]); 
                set_color(COLOR_GREEN, color[GREEN_INDEX]);
                set_color(COLOR_RED, color[RED_INDEX]);
            }
        } 
        else 
        {
            if (flash == pdFALSE) 
            {
                set_color(COLOR_BLUE, 0); 
                set_color(COLOR_GREEN, 0);
                set_color(COLOR_RED, 0);
            }
        }

    }
}

static void flash_task (void * param)
{
    BaseType_t on = pdFALSE; 
    while (1)
    {
        if (flash == pdTRUE)
        {
            set_color(COLOR_BLUE, 0);
            set_color(COLOR_GREEN, 0);
            if (on == pdFALSE)
            {
                set_color(COLOR_RED, 255);
                on = pdTRUE; 
            }
            else 
            {
                set_color(COLOR_RED, 0);
                on = pdFALSE; 
            }
        }

        vTaskDelay(500 / portTICK_PERIOD_MS); 
    }
}

static void lights_setup (void)
{
    light_evt_queue = xQueueCreate(10, sizeof(light_evt_t));
    configASSERT(light_evt_queue); 

    TaskHandle_t color = NULL;
    xTaskCreate(color_update_task, "Color Update", 2048, NULL, 7, &color); 
    configASSERT(color); 

    TaskHandle_t flash_task_handle = NULL;
    xTaskCreate(flash_task, "Flash task", 2048, NULL, 5, &flash_task_handle);
    configASSERT(flash_task_handle); 
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ledc_setup(); 
    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    
    // TaskHandle_t fade = NULL; 
    // xTaskCreate(fade_test, "Fade", 2048, NULL, 7, &fade);
    // configASSERT(fade);

    lights_setup(); 
    encoder_setup(); 

    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();
}
