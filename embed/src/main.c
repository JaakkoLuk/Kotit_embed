#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

// Led pin configurations
static const struct gpio_dt_spec red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

// Red led thread initialization
#define STACKSIZE 500
#define PRIORITY 5
void valo_ledit(void *, void *, void*);
K_THREAD_DEFINE(led_thread, STACKSIZE, valo_ledit, NULL, NULL, NULL, PRIORITY, 0, 0);

//functions split
int init_led(void);
void red_led_on(void);
void red_led_off(void);
void yellow_led_on(void);
void yellow_led_off(void);
void green_led_on(void);
void green_led_off(void);

// Main program
int main(void)
{
    int ret = init_led();
    if (ret < 0) {
        printk("LED initialization failed\n");
        return ret;
    }
    
    printk("LED state machine started\n");
    return 0;
}

// Initialize leds
int init_led(void) 
{
    int ret;
    
    // Initialize red LED
    ret = gpio_pin_configure_dt(&red, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: Red LED configure failed\n");        
        return ret;
    }
    gpio_pin_set_dt(&red, 0);
    
    // Initialize green LED
    ret = gpio_pin_configure_dt(&green, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: Green LED configure failed\n");        
        return ret;
    }
    gpio_pin_set_dt(&green, 0);
    
    // Initialize blue LED (optional, for future use)
    ret = gpio_pin_configure_dt(&blue, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: Blue LED configure failed\n");        
        return ret;
    }
    gpio_pin_set_dt(&blue, 0);
    
    return 0;
}

void valo_ledit(void *arg1, void *arg2, void *arg3) 
{
    printk("LED state machine thread started\n");
    
    while (true) {
        // State 1: Red LED
        red_led_on();
        k_sleep(K_SECONDS(1));
        red_led_off();
        
        // State 2: Yellow LED (Red + Green)
        yellow_led_on();
        k_sleep(K_SECONDS(1));
        yellow_led_off();
        
        // State 3: Green LED
        green_led_on();
        k_sleep(K_SECONDS(1));
        green_led_off();
        //repeat
}
}

void red_led_on(void) 
{
    gpio_pin_set_dt(&red, 1);
    printk("Red ON (State 1)\n");
}

void red_led_off(void) 
{
    gpio_pin_set_dt(&red, 0);
    printk("Red OFF\n");
}

// Yellow LED functions (Red + Green)
void yellow_led_on(void) 
{
    gpio_pin_set_dt(&red, 1);
    gpio_pin_set_dt(&green, 1);
    printk("Yellow ON (State 2) - Red + Green\n");
}

void yellow_led_off(void) 
{
    gpio_pin_set_dt(&red, 0);
    gpio_pin_set_dt(&green, 0);
    printk("Yellow OFF\n");
}

// Green LED functions
void green_led_on(void) 
{
    gpio_pin_set_dt(&green, 1);
    printk("Green ON (State 3)\n");
}

void green_led_off(void) 
{
    gpio_pin_set_dt(&green, 0);
    printk("Green OFF\n");
}