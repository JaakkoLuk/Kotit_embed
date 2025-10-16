#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/timing/timing.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


//robotille lähettäminen ja uart lisätty vanhaan koodiin, testicaset eri tiedostossa.
//errors
#define TIME_LEN_ERROR      -1
#define TIME_ARRAY_ERROR    -2
#define TIME_VALUE_ERROR    -3
#define SEQUENCE_ERROR      -1
#define SEQUENCE_OK         0

// Buffer sizes
#define TIME_BUFFER_SIZE    10
#define SEQ_BUFFER_SIZE     20

// State machine states
typedef enum {
    STATE_IDLE,
    STATE_RECEIVING_TIME,
    STATE_RECEIVING_SEQUENCE,
    STATE_DURATION_MEASURE
} uart_state_t;

// Parser
static int time_parse(char *time) {
    if (time == NULL) return TIME_ARRAY_ERROR;
    if (strlen(time) != 6) return TIME_LEN_ERROR;

    for (int i = 0; i < 6; ++i) {
        if (!isdigit((unsigned char)time[i])) {
            return TIME_LEN_ERROR;
        }
    }

    char ss[3] = { time[4], time[5], '\0' };
    char mm[3] = { time[2], time[3], '\0' };
    char hh[3] = { time[0], time[1], '\0' };

    int second = atoi(ss);
    int minute = atoi(mm);
    int hour   = atoi(hh);

    if (hour > 23 || minute > 59 || second > 59) return TIME_VALUE_ERROR;

    return hour*3600 + minute*60 + second;
}

// Validate LED sequence
static int validate_sequence(char *sequence) {
    if (sequence == NULL) return SEQUENCE_ERROR;
    
    int len = strlen(sequence);
    if (len == 0) return SEQUENCE_ERROR;
    
    for (int i = 0; i < len; i++) {
        char c = toupper((unsigned char)sequence[i]);
        if (c != 'R' && c != 'Y' && c != 'G') {
            return SEQUENCE_ERROR;
        }
    }
    
    return SEQUENCE_OK;
}

static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct device *const uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
#define STACKSIZE 1024
#define PRIORITY 5

struct data_item_t {
    void *fifo_reserved;
    char color;
    timing_t sequence_start_time;
};

K_FIFO_DEFINE(sequence_fifo);

void serial_receive_task(void *arg1, void *arg2, void *arg3);
void dispatcher_task(void *arg1, void *arg2, void *arg3);
void red_led_task(void *arg1, void *arg2, void *arg3);
void yellow_led_task(void *arg1, void *arg2, void *arg3);
void green_led_task(void *arg1, void *arg2, void *arg3);

K_THREAD_DEFINE(serial_thread, STACKSIZE, serial_receive_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(dispatcher_thread, STACKSIZE, dispatcher_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(red_thread, STACKSIZE, red_led_task, NULL, NULL, NULL, PRIORITY+1, 0, 0);
K_THREAD_DEFINE(yellow_thread, STACKSIZE, yellow_led_task, NULL, NULL, NULL, PRIORITY+1, 0, 0);
K_THREAD_DEFINE(green_thread, STACKSIZE, green_led_task, NULL, NULL, NULL, PRIORITY+1, 0, 0);


K_MUTEX_DEFINE(led_mutex);
K_CONDVAR_DEFINE(red_condvar);
K_CONDVAR_DEFINE(yellow_condvar);
K_CONDVAR_DEFINE(green_condvar);
K_SEM_DEFINE(release_sem, 0, 1);

volatile bool red_signal = false;
volatile bool yellow_signal = false;
volatile bool green_signal = false;

// Global variables for sequence timing
static timing_t current_sequence_start_time;

// Timer variables
static char time_buffer[TIME_BUFFER_SIZE];
static char seq_buffer[SEQ_BUFFER_SIZE];
static int buffer_index = 0;
static uart_state_t current_state = STATE_IDLE;

// Duration measurement variables
static timing_t duration_start_time;
static bool measuring_duration = false;

int init_led(void);
int init_uart(void);
void uart_irq_callback(const struct device *dev, void *user_data);
void send_response_to_robot(int value);
void send_duration_to_robot(uint64_t duration_ms);
void handle_uart_character(uint8_t c);

// Timer interrupt handler
void timer_expiry_function(struct k_timer *timer_id) {
    printk("Timer expired! Triggering red LED\n");
    
    struct data_item_t *data = k_malloc(sizeof(struct data_item_t));
    if (data != NULL) {
        data->color = 'R';
        timing_start();
        data->sequence_start_time = timing_counter_get();
        k_fifo_put(&sequence_fifo, data);
    }
}

// Define the timer
K_TIMER_DEFINE(led_timer, timer_expiry_function, NULL);

// Send response to Robot Framework, ei toiminut kun oli esim numeroita ja kirjaimia sekaisin
void send_response_to_robot(int value) {
    char response[20];
    snprintf(response, sizeof(response), "%d", value);
    
    for (int i = 0; i < strlen(response); i++) {
        uart_poll_out(uart_dev, response[i]);
    }
    uart_poll_out(uart_dev, 'X');
}

// Send duration to Robot Framework, ei käytössä
void send_duration_to_robot(uint64_t duration_ms) {
    char response[30];
    snprintf(response, sizeof(response), "%llums", duration_ms);
    
    for (int i = 0; i < strlen(response); i++) {
        uart_poll_out(uart_dev, response[i]);
    }
    
    uart_poll_out(uart_dev, 'X');

}

//toimii nihkeästi
void handle_uart_character(uint8_t c) {
    switch (current_state) {
        case STATE_IDLE:
            if (c == 'T' || c == 't') {
                // Time string command
                current_state = STATE_RECEIVING_TIME;
                buffer_index = 0;
                memset(time_buffer, 0, TIME_BUFFER_SIZE);

                
            } else if (c == 'S' || c == 's') {
                // Sequence command
                current_state = STATE_RECEIVING_SEQUENCE;
                buffer_index = 0;
                memset(seq_buffer, 0, SEQ_BUFFER_SIZE);

                
            } else if (c == 'D' || c == 'd') {
                // Duration measurement command
                current_state = STATE_DURATION_MEASURE;
                timing_init();
                timing_start();
                duration_start_time = timing_counter_get();
                measuring_duration = true;

                
            } else if (c == 'R' || c == 'Y' || c == 'G' || 
                       c == 'r' || c == 'y' || c == 'g') {

                struct data_item_t *data = k_malloc(sizeof(struct data_item_t));
                if (data != NULL) {
                    data->color = (c >= 'a') ? (c - 32) : c;
                    timing_start();
                    data->sequence_start_time = timing_counter_get();
                    k_fifo_put(&sequence_fifo, data);
                }
            }
            break;
            
        case STATE_RECEIVING_TIME:
            if (c == 'X') {
                // End of time string
                time_buffer[buffer_index] = '\0';
                int result = time_parse(time_buffer);
                send_response_to_robot(result);
                
                // If valid, optionally start timer
                if (result > 0) {
                    k_timer_start(&led_timer, K_SECONDS(result), K_NO_WAIT);
                }
                
                current_state = STATE_IDLE;
                
            } else if (buffer_index < TIME_BUFFER_SIZE - 1) {
                time_buffer[buffer_index++] = c;
            } else {
                // Buffer overflow
                send_response_to_robot(TIME_LEN_ERROR);
                current_state = STATE_IDLE;
            }
            break;
            
        case STATE_RECEIVING_SEQUENCE:
            if (c == 'X') {
                // End of sequence
                seq_buffer[buffer_index] = '\0';
                int result = validate_sequence(seq_buffer);
                
                send_response_to_robot(result);
                
                if (result == SEQUENCE_OK) {
                    for (int i = 0; i < strlen(seq_buffer); i++) {
                        struct data_item_t *data = k_malloc(sizeof(struct data_item_t));
                        if (data != NULL) {
                            data->color = toupper((unsigned char)seq_buffer[i]);
                            timing_start();
                            data->sequence_start_time = timing_counter_get();
                            k_fifo_put(&sequence_fifo, data);
                            
                            //testit ei mene läpi ilman delayta
                            k_sleep(K_MSEC(50));
                        }
                    }
                }
                
                current_state = STATE_IDLE;
                
            } else if (buffer_index < SEQ_BUFFER_SIZE - 1) {
                seq_buffer[buffer_index++] = c;
            } else {
                // Buffer overflow
                send_response_to_robot(SEQUENCE_ERROR);
                current_state = STATE_IDLE;
            }
            break;
            
        case STATE_DURATION_MEASURE:
            if (c == 'X') {
                // End duration measurement
                if (measuring_duration) {
                    timing_t duration_end_time = timing_counter_get();
                    timing_stop();
                    uint64_t duration_ns = timing_cycles_to_ns(
                        timing_cycles_get(&duration_start_time, &duration_end_time));
                    uint64_t duration_ms = duration_ns / 1000000;
                    
                    send_duration_to_robot(duration_ms);
                    measuring_duration = false;
                }
                current_state = STATE_IDLE;
            }
            break;
    }
}

// Main program
int main(void)
{
    int ret;
    
    // Initialize timing functionality
    timing_init();
    timing_start();
    
    ret = init_led();
    if (ret < 0) {
        return ret;
    }
    
    ret = init_uart();
    if (ret < 0) {
        return ret;
    }

    return 0;
}

// Initialize all LEDs
int init_led(void) 
{
    int ret;
    
    if (!gpio_is_ready_dt(&red_led)) {
        return -1;
    }
    
    if (!gpio_is_ready_dt(&green_led)) {
        return -1;
    }
    
    if (!gpio_is_ready_dt(&blue_led)) {
        return -1;
    }
    
    ret = gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return ret;
    }
    gpio_pin_set_dt(&red_led, 0);
    
    ret = gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return ret;
    }
    gpio_pin_set_dt(&green_led, 0);
    
    ret = gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return ret;
    }
    gpio_pin_set_dt(&blue_led, 0);
    
    return 0;
}

// Initialize UART
int init_uart(void)
{
    if (!device_is_ready(uart_dev)) {
        return -1;
    }
    
    uart_irq_callback_user_data_set(uart_dev, uart_irq_callback, NULL);
    uart_irq_rx_enable(uart_dev);
    
    return 0;
}

//copilot :D
void uart_irq_callback(const struct device *dev, void *user_data)
{
    uint8_t c;
    
    if (!uart_irq_update(uart_dev)) {
        return;
    }
    
    if (!uart_irq_rx_ready(uart_dev)) {
        return;
    }
    
    while (uart_fifo_read(uart_dev, &c, 1) == 1) {
        handle_uart_character(c);
    }
}

void serial_receive_task(void *arg1, void *arg2, void *arg3)
{
    while (true) {
        k_sleep(K_MSEC(100));
    }
}

void dispatcher_task(void *arg1, void *arg2, void *arg3)
{
    printk("Dispatcher task started\n");
    
    while (true) {
        // Wait for item in FIFO
        struct data_item_t *data = k_fifo_get(&sequence_fifo, K_FOREVER);
        
        if (data != NULL) {
            current_sequence_start_time = data->sequence_start_time;
            
            k_mutex_lock(&led_mutex, K_FOREVER);
            
            switch (data->color) {
                case 'R':
                    red_signal = true;
                    k_condvar_signal(&red_condvar);
                    break;
                case 'Y':
                    yellow_signal = true;
                    k_condvar_signal(&yellow_condvar);
                    break;
                case 'G':
                    green_signal = true;
                    k_condvar_signal(&green_condvar);
                    break;
                default:
                    break;
            }
            
            k_mutex_unlock(&led_mutex);
            
            k_free(data);
            
            // Wait for the light task to complete
            k_sem_take(&release_sem, K_FOREVER);
            
            // Calculate and print total sequence time
            timing_t sequence_end_time = timing_counter_get();
            timing_stop();
            uint64_t total_sequence_ns = timing_cycles_to_ns(timing_cycles_get(&current_sequence_start_time, &sequence_end_time));
            printk("TOTAL SEQUENCE TIME: %lld ns (%lld us)\n", total_sequence_ns, total_sequence_ns / 1000);
        }
    }
}


void red_led_task(void *arg1, void *arg2, void *arg3)
{
    printk("Red LED task started\n");
    
    while (true) {
        k_mutex_lock(&led_mutex, K_FOREVER);
        
        while (!red_signal) {
            k_condvar_wait(&red_condvar, &led_mutex, K_FOREVER);
        }
        
        red_signal = false;
        k_mutex_unlock(&led_mutex);
        
        timing_start();
        timing_t red_start_time = timing_counter_get();
        
        gpio_pin_set_dt(&red_led, 1);
        k_sleep(K_SECONDS(1));
        gpio_pin_set_dt(&red_led, 0);
        
        timing_t red_end_time = timing_counter_get();
        timing_stop();
        uint64_t red_sequence_ns = timing_cycles_to_ns(timing_cycles_get(&red_start_time, &red_end_time));
        printk("RED LED SEQUENCE TIME: %lld ns (%lld us)\n", red_sequence_ns, red_sequence_ns / 1000);
        
        k_sem_give(&release_sem);
    }
}

void yellow_led_task(void *arg1, void *arg2, void *arg3)
{
    while (true) {
        k_mutex_lock(&led_mutex, K_FOREVER);
        
        while (!yellow_signal) {
            k_condvar_wait(&yellow_condvar, &led_mutex, K_FOREVER);
        }
        
        yellow_signal = false;
        k_mutex_unlock(&led_mutex);
        
        timing_start();
        timing_t yellow_start_time = timing_counter_get();
        
        gpio_pin_set_dt(&red_led, 1);
        gpio_pin_set_dt(&green_led, 1);
        k_sleep(K_SECONDS(1));
        gpio_pin_set_dt(&red_led, 0);
        gpio_pin_set_dt(&green_led, 0);
        
        timing_t yellow_end_time = timing_counter_get();
        timing_stop();
        uint64_t yellow_sequence_ns = timing_cycles_to_ns(timing_cycles_get(&yellow_start_time, &yellow_end_time));
        printk("YELLOW LED SEQUENCE TIME: %lld ns (%lld us)\n", yellow_sequence_ns, yellow_sequence_ns / 1000);
        
        k_sem_give(&release_sem);
    }
}

void green_led_task(void *arg1, void *arg2, void *arg3)
{
    printk("Green LED task started\n");
    
    while (true) {
        k_mutex_lock(&led_mutex, K_FOREVER);
        
        while (!green_signal) {
            k_condvar_wait(&green_condvar, &led_mutex, K_FOREVER);
        }
        
        green_signal = false;
        k_mutex_unlock(&led_mutex);
        
        timing_start();
        timing_t green_start_time = timing_counter_get();
        
        gpio_pin_set_dt(&green_led, 1);
        k_sleep(K_SECONDS(1));
        gpio_pin_set_dt(&green_led, 0);
        
        timing_t green_end_time = timing_counter_get();
        timing_stop();
        uint64_t green_sequence_ns = timing_cycles_to_ns(timing_cycles_get(&green_start_time, &green_end_time));
        printk("GREEN LED SEQUENCE TIME: %lld ns (%lld us)\n", green_sequence_ns, green_sequence_ns / 1000);
        
        k_sem_give(&release_sem);
    }
}