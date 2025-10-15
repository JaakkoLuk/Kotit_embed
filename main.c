#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/timing/timing.h>
#include <stdlib.h>
#include <string.h>

//errors
#define TIME_LEN_ERROR      -1
#define TIME_ARRAY_ERROR    -2
#define TIME_VALUE_ERROR    -3

//lisätty parseri ja aikakeskeytys vanhaan koodiin, esim t000006 punainen ledi syttyy 6sek jälkeen, nuo muutamat lisätestikeissit kanssa lisätty h tiedostoon
//! HOX, testailin kanssa tällä vscoden copilotilla, millaisia testicaseja se keksii(nuo TimeParser2 tiedostot), niistä ei tarvihe välittää



// Parser
static int time_parse(char *time) {
    if (time == NULL) return TIME_ARRAY_ERROR;
    if (strlen(time) != 6) return TIME_LEN_ERROR;

    for (int i = 0; i < 6; ++i) {
        unsigned char uc = (unsigned char)time[i];
        if (uc < '0' || uc > '9') return TIME_LEN_ERROR;
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
#define TIME_BUFFER_SIZE 10
static char time_buffer[TIME_BUFFER_SIZE];
static int time_index = 0;
static bool receiving_time = false;

int init_led(void);
int init_uart(void);
void uart_irq_callback(const struct device *dev, void *user_data);

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

// Main program
int main(void)
{
    int ret;
    
    // Initialize timing functionality
    timing_init();
    timing_start();
    //printk("Timing system start\n");
    
    ret = init_led();
    if (ret < 0) {
        //printk("LED init failed\n");
        return ret;
    }
    
    ret = init_uart();
    if (ret < 0) {
        //printk("UART init failed\n");
        return ret;
    }
    
    printk("Traffic light started\n");
    printk("Commands: R/Y/G for immediate, T+6digits for timer\n");
    printk("Example: T000005 = 5 second timer\n");

    return 0;
}

// Initialize all LEDs
int init_led(void) 
{
    int ret;
    
    // Check if LED devices are ready
    if (!gpio_is_ready_dt(&red_led)) {
        //printk("Error: Red LED device not ready\n");
        return -1;
    }
    
    if (!gpio_is_ready_dt(&green_led)) {
        //printk("Error: Green LED device not ready\n");
        return -1;
    }
    
    if (!gpio_is_ready_dt(&blue_led)) {
        //printk("Error: Blue LED device not ready\n");
        return -1;
    }
    
    // Initialize red LED
    ret = gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        //printk("Error: Red failed\n");        
        return ret;
    }
    gpio_pin_set_dt(&red_led, 0);
    
    // Initialize green LED
    ret = gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        //printk("Error: Green failed\n");        
        return ret;
    }
    gpio_pin_set_dt(&green_led, 0);
    
    // Initialize blue LED
    ret = gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        //printk("Error: blue failed\n");        
        return ret;
    }
    gpio_pin_set_dt(&blue_led, 0);
    
    //printk("All LEDs initialized successfully\n");
    return 0;
}

// Initialize UART
int init_uart(void)
{
    if (!device_is_ready(uart_dev)) {
        //printk("UART device not ready\n");
        return -1;
    }
    
    uart_irq_callback_user_data_set(uart_dev, uart_irq_callback, NULL);
    uart_irq_rx_enable(uart_dev);
    
    //printk("UART initialized successfully\n");
    return 0;
}

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
        // Check for time command 'T'
        if (c == 'T' || c == 't') {
            receiving_time = true;
            time_index = 0;
            memset(time_buffer, 0, TIME_BUFFER_SIZE);
            printk("Receiving time...\n");
            continue;
        }
        
        // Receive time digits
        if (receiving_time) {
            if (c >= '0' && c <= '9' && time_index < 6) {
                time_buffer[time_index++] = c;
                
                if (time_index == 6) {
                    time_buffer[6] = '\0';
                    receiving_time = false;
                    
                    printk("Time received: %s\n", time_buffer);
                    
                    int seconds = time_parse(time_buffer);
                    
                    if (seconds > 0) {
                        printk("Starting timer for %d seconds\n", seconds);
                        k_timer_start(&led_timer, K_SECONDS(seconds), K_NO_WAIT);
                    } else {
                        printk("Invalid time: error %d\n", seconds);
                    }
                }
            } else {
                receiving_time = false;
                time_index = 0;
            }
            continue;
        }
        
        // Handle regular LED commands
        if (c == 'R' || c == 'Y' || c == 'G' || c == 'r' || c == 'y' || c == 'g') {
            struct data_item_t *data = k_malloc(sizeof(struct data_item_t));
            if (data != NULL) {
                data->color = (c >= 'a') ? (c - 32) : c;
                timing_start();
                data->sequence_start_time = timing_counter_get();
                k_fifo_put(&sequence_fifo, data);
                //printk("Received: %c\n", data->color);
            } else {
                //printk("Failed to allocate memory for FIFO item\n");
            }
        }
    }
}

void serial_receive_task(void *arg1, void *arg2, void *arg3)
{
    //printk("Serial task started\n");
    
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
            //printk("Dispatcher processing: %c\n", data->color);
            
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
                    //printk("Unknown color: %c\n", data->color);
                    break;
            }
            
            k_mutex_unlock(&led_mutex);
            
            k_free(data);
            
            // Wait for the light task to complete
            k_sem_take(&release_sem, K_FOREVER);
            //printk("Dispatcher: Light task completed\n");
            
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
        // Wait for signal from dispatcher
        k_mutex_lock(&led_mutex, K_FOREVER);
        
        while (!red_signal) {
            k_condvar_wait(&red_condvar, &led_mutex, K_FOREVER);
        }
        
        // Clear signal flag
        red_signal = false;
        k_mutex_unlock(&led_mutex);
        
        // Start timing the LED sequence
        timing_start();
        timing_t red_start_time = timing_counter_get();
        
        // Execute red LED sequence
        //printk("Red LED: Starting sequence\n");
        gpio_pin_set_dt(&red_led, 1);
        //printk("Red ON\n");
        k_sleep(K_SECONDS(1));
        gpio_pin_set_dt(&red_led, 0);
        //printk("Red OFF\n");
        
        // Calculate and print individual LED sequence time
        timing_t red_end_time = timing_counter_get();
        timing_stop();
        uint64_t red_sequence_ns = timing_cycles_to_ns(timing_cycles_get(&red_start_time, &red_end_time));
        printk("RED LED SEQUENCE TIME: %lld ns (%lld us)\n", red_sequence_ns, red_sequence_ns / 1000);
        
        // Send release signal to dispatcher
        k_sem_give(&release_sem);
        //printk("Red LED: Sequence completed\n");
    }
}

void yellow_led_task(void *arg1, void *arg2, void *arg3)
{
    //printk("Yellow LED task started\n");
    
    while (true) {
        k_mutex_lock(&led_mutex, K_FOREVER);
        
        while (!yellow_signal) {
            k_condvar_wait(&yellow_condvar, &led_mutex, K_FOREVER);
        }
        
        yellow_signal = false;
        k_mutex_unlock(&led_mutex);
        
        // Start timing the LED sequence
        timing_start();
        timing_t yellow_start_time = timing_counter_get();
        
        //printk("Yellow LED: Starting sequence\n");
        gpio_pin_set_dt(&red_led, 1);
        gpio_pin_set_dt(&green_led, 1);
        //printk("Yellow ON (Red + Green)\n");
        k_sleep(K_SECONDS(1));
        gpio_pin_set_dt(&red_led, 0);
        gpio_pin_set_dt(&green_led, 0);
        //printk("Yellow OFF\n");
        
        // Calculate and print individual LED sequence time
        timing_t yellow_end_time = timing_counter_get();
        timing_stop();
        uint64_t yellow_sequence_ns = timing_cycles_to_ns(timing_cycles_get(&yellow_start_time, &yellow_end_time));
        printk("YELLOW LED SEQUENCE TIME: %lld ns (%lld us)\n", yellow_sequence_ns, yellow_sequence_ns / 1000);
        
        k_sem_give(&release_sem);
        //printk("Yellow LED: Sequence completed\n");
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
        
        // Clear signal flag
        green_signal = false;
        k_mutex_unlock(&led_mutex);
        
        // Start timing the LED sequence
        timing_start();
        timing_t green_start_time = timing_counter_get();
        
        // Execute green LED sequence
        //printk("Green LED: Starting sequence\n");
        gpio_pin_set_dt(&green_led, 1);
        //printk("Green ON\n");
        k_sleep(K_SECONDS(1));
        gpio_pin_set_dt(&green_led, 0);
        //printk("Green OFF\n");
        
        // Calculate and print individual LED sequence time
        timing_t green_end_time = timing_counter_get();
        timing_stop();
        uint64_t green_sequence_ns = timing_cycles_to_ns(timing_cycles_get(&green_start_time, &green_end_time));
        printk("GREEN LED SEQUENCE TIME: %lld ns (%lld us)\n", green_sequence_ns, green_sequence_ns / 1000);
        
        // Send release signal to dispatcher
        k_sem_give(&release_sem);
        //printk("Green LED: Sequence completed\n");
    }
}