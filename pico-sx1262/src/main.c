/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 * This example uses OTAA to join the LoRaWAN network and then sends the 
 * internal temperature sensors value up as an uplink message periodically 
 * and the first byte of any uplink messages received controls the boards
 * built-in LED.
 */

#include <stdio.h>
#include <string.h>


#include "hardware/adc.h"
#include "hardware/gpio.h"

#include "pico/stdlib.h"
#include "pico/lorawan.h"
#include "tusb.h"

#include "dht.h"

// edit with LoRaWAN Node Region and OTAA settings 
#include "config.h"

static const dht_model_t DHT_MODEL = DHT22;
static const uint DATA_PIN = 14;

// pin configuration for SX12xx radio module
const struct lorawan_sx12xx_settings sx12xx_settings = {
    .spi = {
        .inst = spi1,
        .mosi = 11,
        .miso = 12,
        .sck  = 10,
        .nss  = 3
    },
    .reset = 15,
    .busy = 2,
    // sx127x would use dio0 pin, and sx126x dont use it 
    // .dio0  = 7,
    .dio1  = 20
};

// OTAA settings
const struct lorawan_otaa_settings otaa_settings = {
    .device_eui   = LORAWAN_DEVICE_EUI,
    .app_eui      = LORAWAN_APP_EUI,
    .app_key      = LORAWAN_APP_KEY,
    .channel_mask = LORAWAN_CHANNEL_MASK
};

// variables for receiving data
int receive_length = 0;
uint8_t receive_buffer[242];
uint8_t receive_port = 0;

// functions used in main
void internal_temperature_init();
float internal_temperature_get();

int main( void )
{
    // initialize stdio and wait for USB CDC connect
    stdio_init_all();
    while (!tud_cdc_connected()) {
        tight_loop_contents();
    }
    
    printf("Pico LoRaWAN - OTAA - Temperature + LED\n\n");

    // initialize the LED pin and internal temperature ADC
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    internal_temperature_init();

    // uncomment next line to enable debug
    // lorawan_debug(true);

    // initialize the LoRaWAN stack
    printf("Initilizating LoRaWAN ... ");
    if (lorawan_init_otaa(&sx12xx_settings, LORAWAN_REGION, &otaa_settings) < 0) {
        printf("failed!!!\n");
        while (1) {
            tight_loop_contents();
        }
    } else {
        printf("success!\n");
    }

    // Start the join process and wait
    printf("Joining LoRaWAN network ...");
    lorawan_join();

    while (!lorawan_is_joined()) {
        lorawan_process_timeout_ms(1000);
        printf(".");
    }
    printf(" joined successfully!\n");
    

    dht_t dht;
    dht_init(&dht, DHT_MODEL, pio0, DATA_PIN, true /* pull_up */);
    // loop forever
    while (1) {
        // get the internal temperature
        int16_t adc_temperature_byte = internal_temperature_get() * 10;

        // send the internal temperature as a (signed) byte in an unconfirmed uplink message
        printf("sending internal temperature: %d 'C (0x%02x)... ", adc_temperature_byte, adc_temperature_byte);


        // get DHT22 values
        dht_start_measurement(&dht);
        

        float humidity;
        float temperature_c;
        dht_result_t result = dht_finish_measurement_blocking(&dht, &humidity, &temperature_c);
        if (result == DHT_RESULT_OK) {
            printf("%.1f C, %.1f%% humidity\n", temperature_c, humidity);
        } else if (result == DHT_RESULT_TIMEOUT) {
            puts("DHT sensor not responding. Please check your wiring.");
        } else {
            assert(result == DHT_RESULT_BAD_CHECKSUM);
            puts("Bad checksum");
        }

        unsigned char bytearray[11];

        // internal temperature
        char cayenne_lpp_string[13];
        sprintf(cayenne_lpp_string, "%02x67%04x", 1, adc_temperature_byte);
        printf("sending hex value (internal temperature): %s\n", cayenne_lpp_string);
        unsigned long cayenne_lpp_int_tmp = (int)strtol(cayenne_lpp_string, NULL, 16);

        bytearray[0] = 0x01;
        bytearray[1] = 0x67;
        bytearray[2] = cayenne_lpp_int_tmp >> 8;
        bytearray[3] = cayenne_lpp_int_tmp & 0xff;

        // DHT22 temperature
        char cayenne_lpp_string_temperature[13];
        int16_t temperature = (int)(temperature_c*10);
        sprintf(cayenne_lpp_string_temperature, "%02x67%04x", 2, temperature);
        printf("sending hex value (dht22 temperature): %s\n", cayenne_lpp_string_temperature);
        unsigned long cayenne_lpp_int_tmp_temperature = (int)strtol(cayenne_lpp_string_temperature, NULL, 16);
        bytearray[4] = 0x02;
        bytearray[5] = 0x67;
        bytearray[6] = cayenne_lpp_int_tmp_temperature >> 8;
        bytearray[7] = cayenne_lpp_int_tmp_temperature & 0xff;


        // DHT22 humidity
        char cayenne_lpp_string_humidity[13];
        int16_t humidity_v = (int)(humidity*2);
        sprintf(cayenne_lpp_string_humidity, "%02x68%02x", 3, humidity_v);
        printf("sending hex value (dht22 humidity): %s\n", cayenne_lpp_string_humidity);
        unsigned long cayenne_lpp_int_tmp_humidity = (int)strtol(cayenne_lpp_string_humidity, NULL, 16);
        bytearray[8] = 0x03;
        bytearray[9] = 0x68;
        bytearray[10] = cayenne_lpp_int_tmp_humidity;
        
        if (lorawan_send_unconfirmed(&bytearray, sizeof(bytearray), 2) < 0) {
            printf("failed!!!\n");
        } else {
            printf("success!\n");
        }

        // wait for up to 30 seconds for an event
        if (lorawan_process_timeout_ms(30000) == 0) {
            // check if a downlink message was received
            receive_length = lorawan_receive(receive_buffer, sizeof(receive_buffer), &receive_port);
            if (receive_length > -1) {
                printf("received a %d byte message on port %d: ", receive_length, receive_port);

                for (int i = 0; i < receive_length; i++) {
                    printf("%02x", receive_buffer[i]);
                }
                printf("\n");

                // the first byte of the received message controls the on board LED
                gpio_put(PICO_DEFAULT_LED_PIN, receive_buffer[0]);
            }
        }
        // sleep 5 minutes
        sleep_ms(300000);
    }

    return 0;
}

void internal_temperature_init()
{
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
}

float internal_temperature_get()
{
    const float v_ref = 3.3;

    // select and read the ADC
    adc_select_input(4);
    uint16_t adc_raw = adc_read();

    // convert the raw ADC value to a voltage
    float adc_voltage = adc_raw * v_ref / 4095.0f;

    // convert voltage to temperature, using the formula from 
    // section 4.9.4 in the RP2040 datasheet
    //   https://datasheets.raspberrypi.org/rp2040/rp2040-datasheet.pdf
    float adc_temperature = 27.0 - ((adc_voltage - 0.706) / 0.001721);

    return adc_temperature;
}
