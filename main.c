#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);
static const struct device *const uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
#define STACKSIZE 1024
#define PRIORITY 5


struct data_item_t {
    void *fifo_reserved;
    char color;
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


int init_led(void);
int init_uart(void);
void uart_irq_callback(const struct device *dev, void *user_data);

// Main program
int main(void)
{
    int ret;
    
    ret = init_led();
    if (ret < 0) {
        printk("LED initialization failed\n");
        return ret;
    }
    
    ret = init_uart();
    if (ret < 0) {
        printk("UART initialization failed\n");
        return ret;
    }
    
    printk("LED sequence started\n");

    return 0;
}

// Initialize all LEDs
int init_led(void) 
{
    int ret;
    
    // Check if LED devices are ready
    if (!gpio_is_ready_dt(&red_led)) {
        printk("Error: Red LED device not ready\n");
        return -1;
    }
    
    if (!gpio_is_ready_dt(&green_led)) {
        printk("Error: Green LED device not ready\n");
        return -1;
    }
    
    if (!gpio_is_ready_dt(&blue_led)) {
        printk("Error: Blue LED device not ready\n");
        return -1;
    }
    
    // Initialize red LED
    ret = gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: Red failed\n");        
        return ret;
    }
    gpio_pin_set_dt(&red_led, 0);
    
    // Initialize green LED
    ret = gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: Green failed\n");        
        return ret;
    }
    gpio_pin_set_dt(&green_led, 0);
    
    // Initialize blue LED
    ret = gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: blue failed\n");        
        return ret;
    }
    gpio_pin_set_dt(&blue_led, 0);
    
    printk("All LEDs initialized successfully\n");
    return 0;
}

// Initialize UART
int init_uart(void)
{
    if (!device_is_ready(uart_dev)) {
        printk("UART device not ready\n");
        return -1;
    }
    
    uart_irq_callback_user_data_set(uart_dev, uart_irq_callback, NULL);
    uart_irq_rx_enable(uart_dev);
    
    printk("UART initialized successfully\n");
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
        if (c == 'R' || c == 'Y' || c == 'G' || c == 'r' || c == 'y' || c == 'g') {
            struct data_item_t *data = k_malloc(sizeof(struct data_item_t));
            if (data != NULL) {
                data->color = (c >= 'a') ? (c - 32) : c; // Convert to uppercase
                k_fifo_put(&sequence_fifo, data);
                printk("Received: %c\n", data->color);
            } else {
                printk("Failed to allocate memory for FIFO item\n");
            }
        }
    }
}

void serial_receive_task(void *arg1, void *arg2, void *arg3)
{
    printk("Serial task started\n");
    
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
            printk("Dispatcher processing: %c\n", data->color);
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
                    printk("Unknown color: %c\n", data->color);
                    break;
            }
            
            k_mutex_unlock(&led_mutex);
            
            k_free(data);
            
            // Wait for the light task to complete
            k_sem_take(&release_sem, K_FOREVER);
            printk("Dispatcher: Light task completed\n");
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
        
        // Execute red LED sequence
        printk("Red LED: Starting sequence\n");
        gpio_pin_set_dt(&red_led, 1);
        printk("Red ON\n");
        k_sleep(K_SECONDS(1));
        gpio_pin_set_dt(&red_led, 0);
        printk("Red OFF\n");
        
        // Send release signal to dispatcher
        k_sem_give(&release_sem);
        printk("Red LED: Sequence completed\n");
    }
}

void yellow_led_task(void *arg1, void *arg2, void *arg3)
{
    printk("Yellow LED task started\n");
    
    while (true) {
        k_mutex_lock(&led_mutex, K_FOREVER);
        
        while (!yellow_signal) {
            k_condvar_wait(&yellow_condvar, &led_mutex, K_FOREVER);
        }
        
        // Clear signal flag
        yellow_signal = false;
        k_mutex_unlock(&led_mutex);
        
        printk("Yellow LED: Starting sequence\n");
        gpio_pin_set_dt(&red_led, 1);
        gpio_pin_set_dt(&green_led, 1);
        printk("Yellow ON (Red + Green)\n");
        k_sleep(K_SECONDS(1));
        gpio_pin_set_dt(&red_led, 0);
        gpio_pin_set_dt(&green_led, 0);
        printk("Yellow OFF\n");
        
        k_sem_give(&release_sem);
        printk("Yellow LED: Sequence completed\n");
    }
}

void green_led_task(void *arg1, void *arg2, void *arg3)
{
    printk("Green LED task started\n");
    
    while (true) {
        // Wait for signal from dispatcher
        k_mutex_lock(&led_mutex, K_FOREVER);
        
        while (!green_signal) {
            k_condvar_wait(&green_condvar, &led_mutex, K_FOREVER);
        }
        
        // Clear signal flag
        green_signal = false;
        k_mutex_unlock(&led_mutex);
        
        // Execute green LED sequence
        printk("Green LED: Starting sequence\n");
        gpio_pin_set_dt(&green_led, 1);
        printk("Green ON\n");
        k_sleep(K_SECONDS(1));
        gpio_pin_set_dt(&green_led, 0);
        printk("Green OFF\n");
        
        // Send release signal to dispatcher
        k_sem_give(&release_sem);
        printk("Green LED: Sequence completed\n");
    }
}