#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

//ledit, sininen jäi tuonne, mutta olkoon.
//muutettu vk5 pohjilta, muokattu tilakonetta ja lisätty napit ynnä muut härpäkkeet.

static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);
static const struct gpio_dt_spec blue_led = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);

//napit
static const struct gpio_dt_spec button_0 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct gpio_dt_spec button_1 = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static const struct gpio_dt_spec button_2 = GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios);
static const struct gpio_dt_spec button_3 = GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios);
static const struct gpio_dt_spec button_4 = GPIO_DT_SPEC_GET(DT_ALIAS(sw4), gpios);

//gpio
static struct gpio_callback button_0_cb_data;
static struct gpio_callback button_1_cb_data;
static struct gpio_callback button_2_cb_data;
static struct gpio_callback button_3_cb_data;
static struct gpio_callback button_4_cb_data;

//tilakoneen tilat
#define STATE_RED       0
#define STATE_YELLOW    1
#define STATE_GREEN     2
#define STATE_PAUSE     4
#define STATE_FLASH_YELLOW 5

//globaalit tilamuuttujat
static volatile int led_state = STATE_RED;
static volatile int saved_state = STATE_RED;
static volatile bool manual_red = false;
static volatile bool manual_yellow = false;
static volatile bool manual_green = false;

//funktioiden määrittely
int init_leds(void);
int init_buttons(void);
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void button_4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void set_led_color(int state);
void clear_all_leds(void);

//ledien alustus
int init_leds(void) {
    int ret;
    if (!gpio_is_ready_dt(&red_led)) {
        printk("Error: Red LED device not ready\n");
        return -1;
    }
    
    if (!gpio_is_ready_dt(&green_led)) {
        printk("Error: Green LED device not ready\n");
        return -1;
    }
    
    if (!gpio_is_ready_dt(&blue_led)) {
        printk("Error: yellow LED device not ready\n");
        return -1;
    }
    
    //punainen
    ret = gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: Red LED config failed\n");
        return ret;
    }
    gpio_pin_set_dt(&red_led, 0);
    
    //vihreä
    ret = gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: Green LED config failed\n");
        return ret;
    }
    gpio_pin_set_dt(&green_led, 0);
    
    //sininen
    ret = gpio_pin_configure_dt(&blue_led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: yellow LED config failed\n");
        return ret;
    }
    gpio_pin_set_dt(&blue_led, 0);
    
    printk("LEDs initialized\n");
    return 0;
}

// nappien alustus
int init_buttons(void) {
    int ret;
    
    // Button 0 - Pause functionality
    if (!gpio_is_ready_dt(&button_0)) {
        printk("Error: Button 0 not ready\n");
        return -1;
    }
    
    ret = gpio_pin_configure_dt(&button_0, GPIO_INPUT);
    if (ret < 0) {
        printk("Error: Button 0 config failed\n");
        return ret;
    }
    
    ret = gpio_pin_interrupt_configure_dt(&button_0, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        printk("Error: Button 0 interrupt config failed\n");
        return ret;
    }
    
    gpio_init_callback(&button_0_cb_data, button_0_handler, BIT(button_0.pin));
    gpio_add_callback(button_0.port, &button_0_cb_data);
    
    //1 nappi, manuaalinen
    if (!gpio_is_ready_dt(&button_1)) {
        printk("Error: Button 1 not ready\n");
        return -1;
    }
    ret = gpio_pin_configure_dt(&button_1, GPIO_INPUT);
    if (ret < 0) {
        return ret;
    }
    ret = gpio_pin_interrupt_configure_dt(&button_1, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        return ret;
    }
    
    gpio_init_callback(&button_1_cb_data, button_1_handler, BIT(button_1.pin));
    gpio_add_callback(button_1.port, &button_1_cb_data);
    
    //2 nappi manuaalinen
    if (!gpio_is_ready_dt(&button_2)) {
        printk("Error: Button 2 not ready\n");
        return -1;
    }
    ret = gpio_pin_configure_dt(&button_2, GPIO_INPUT);
    if (ret < 0) {
        return ret;
    }
    ret = gpio_pin_interrupt_configure_dt(&button_2, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        return ret;
    }
    
    gpio_init_callback(&button_2_cb_data, button_2_handler, BIT(button_2.pin));
    gpio_add_callback(button_2.port, &button_2_cb_data);
    
    //3 nappi manuaalinen
    if (!gpio_is_ready_dt(&button_3)) {
        printk("Error: Button 3 not ready\n");
        return -1;
    }
    ret = gpio_pin_configure_dt(&button_3, GPIO_INPUT);
    if (ret < 0) {
        return ret;
    }
    ret = gpio_pin_interrupt_configure_dt(&button_3, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        return ret;
    }
    
    gpio_init_callback(&button_3_cb_data, button_3_handler, BIT(button_3.pin));
    gpio_add_callback(button_3.port, &button_3_cb_data);
    
    //villkkuva härpäke
    if (!gpio_is_ready_dt(&button_4)) {
        printk("Error: Button 4 not ready\n");
        return -1;
    }
    ret = gpio_pin_configure_dt(&button_4, GPIO_INPUT);
    if (ret < 0) {
        return ret;
    }
    ret = gpio_pin_interrupt_configure_dt(&button_4, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        return ret;
    }
    
    gpio_init_callback(&button_4_cb_data, button_4_handler, BIT(button_4.pin));
    gpio_add_callback(button_4.port, &button_4_cb_data);
    return 0;
}

//stop/start nappi
void button_0_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    printk("nappi 0 painettu stop/start\n");
    
    if (led_state == STATE_PAUSE) {
        // Resume: restore saved state
        led_state = saved_state;
        printk("Resuming state: %d\n", led_state);
    } else {
        // Pause: save current state
        saved_state = led_state;
        led_state = STATE_PAUSE;
        printk("Paused at state: %d\n", saved_state);
    }
}

//1 nappi handler
void button_1_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    printk("nappi 1 painettu - Vaihda punainen\n");
    manual_red = !manual_red;
    gpio_pin_set_dt(&red_led, manual_red ? 1 : 0);
}

//2 nappi handler
void button_2_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    printk("nappi 2 painettu - Vaihda keltainen\n");
    manual_yellow = !manual_yellow;
    if (manual_yellow) {
        gpio_pin_set_dt(&red_led, 1);
        gpio_pin_set_dt(&green_led, 1);
    } else {
        gpio_pin_set_dt(&red_led, 0);
        gpio_pin_set_dt(&green_led, 0);
    }
}

//3 nappi handler
void button_3_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    printk("nappi 3 painettu - Vaihda vihreä\n");
    manual_green = !manual_green;
    gpio_pin_set_dt(&green_led, manual_green ? 1 : 0);
}

//4 nappi handler
void button_4_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    printk("nappi 4 painettu - Villkkuva keltainen tila\n");
    
    if (led_state == STATE_FLASH_YELLOW) {
        // Exit flashing mode, return to red
        led_state = STATE_RED;
        printk("Poistutaan vilkkuvasta keltaisesta tilasta\n");
    } else {
        // Enter flashing yellow mode
        saved_state = led_state;
        led_state = STATE_FLASH_YELLOW;
        printk("Siirrytään vilkkuvaan keltaiseen tilaan\n");
    }
}

// Clear all LEDs
void clear_all_leds(void) {
    gpio_pin_set_dt(&red_led, 0);
    gpio_pin_set_dt(&green_led, 0);
    gpio_pin_set_dt(&blue_led, 0);
}

// Set LED color based on state
void set_led_color(int state) {
    clear_all_leds();
    
    switch (state) {
        case STATE_RED:
            gpio_pin_set_dt(&red_led, 1);
            break;
        case STATE_YELLOW:
            gpio_pin_set_dt(&red_led, 1);
            gpio_pin_set_dt(&green_led, 1);
            break;
        case STATE_GREEN:
            gpio_pin_set_dt(&green_led, 1);
            break;
        case STATE_PAUSE:
            // Keep current state
            break;
        case STATE_FLASH_YELLOW:
            // Handled separately inside main
            break;
    }
}

// Main
int main(void) {
    int ret;
    bool flash_state = false;
    
    // Initialize LEDs
    ret = init_leds();
    if (ret < 0) {
        printk("LED initialization failed\n");
        return ret;
    }
    
    // Initialize buttons
    ret = init_buttons();
    if (ret < 0) {
        printk("Button initialization failed\n");
        return ret;
    }
    
    
    //tilakone loop, vilkkuva välillä rikkoo tämän
    while (1) {
        if (led_state == STATE_FLASH_YELLOW) {
            flash_state = !flash_state;
            if (flash_state) {
                gpio_pin_set_dt(&red_led, 1);
                gpio_pin_set_dt(&green_led, 1);
            } else {
                gpio_pin_set_dt(&red_led, 0);
                gpio_pin_set_dt(&green_led, 0);
            }
            k_sleep(K_MSEC(500)); //vilkku
            continue;
        }
        
        //stop tila
        if (led_state == STATE_PAUSE) {
            k_sleep(K_MSEC(100));
            continue;
        }
        
        //start tila
        switch (led_state) {
            case STATE_RED:
                printk("State: RED\n");
                set_led_color(STATE_RED);
                k_sleep(K_SECONDS(2));
                
                // jos pause ei päällä
                if (led_state != STATE_PAUSE) {
                    led_state = STATE_YELLOW;
                }
                break;
                
            case STATE_YELLOW:
                printk("State: YELLOW\n");
                set_led_color(STATE_YELLOW);
                k_sleep(K_SECONDS(2));

                // jos pause ei päällä
                if (led_state != STATE_PAUSE) {
                    led_state = STATE_GREEN;
                }
                break;
                
            case STATE_GREEN:
                printk("State: GREEN\n");
                set_led_color(STATE_GREEN);
                k_sleep(K_SECONDS(2));

                // jos pause ei päällä
                if (led_state != STATE_PAUSE) {
                    led_state = STATE_RED;
                }
                break;
                
            default:
                led_state = STATE_RED;
                break;
        }
    }
    
    return 0;
}