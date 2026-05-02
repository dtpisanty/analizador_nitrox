#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

#define ADS1115_ADDR 0b1001000
#define CONV_REG 0x00
#define CONF_REG 0x01
#define READINGS 100

void ads1115_init(){
    i2c_init(I2C_PORT, 100*1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    // gpio_pull_up(I2C_SDA);
    // gpio_pull_up(I2C_SCL);
}

int16_t ads1115_read_ch(uint8_t channel){
    uint16_t config=(1<<15)|
                    ((4+channel)<<12)|
                    (0b101<<9)| //change to 101 for o2 sensor
                    (0b1<<8)|
                    (0b100<<5)|
                    (0b0<<4)|
                    (0b0<<3)|
                    (0b0<<2)|
                    (0b11);
    uint8_t buffer[3];
    buffer[0]=CONF_REG;
    buffer[1]=(config>>8) & 0xFF;
    buffer[2]=config & 0xFF;
    i2c_write_blocking(I2C_PORT,ADS1115_ADDR,buffer,3,false);
    sleep_ms(10);
    uint8_t reg=CONV_REG;
    i2c_write_blocking(I2C_PORT,ADS1115_ADDR,&reg,1,false);
    uint8_t raw[2];
    i2c_read_blocking(I2C_PORT,ADS1115_ADDR,raw,2,false);
    return (uint16_t)((raw[0]<<8)| raw[1]);
}

float reading_to_mv(uint16_t reading){
    return reading*0.007813f; //change to 0.007813f for o2 sensor
}

float variance(float *list, int length,float mean){
    float sq_diff=0;
    for(int i=0;i<length;i++){
        sq_diff+=(list[i]-mean)*(list[i]-mean);
    }
    return sq_diff/length;
}

int main()
{
    stdio_init_all();
    ads1115_init();
   
    while (true) {
        float millivolts[READINGS];
        float mean_mv=0;
        for(int i=0;i<READINGS;i++){
            uint16_t val=ads1115_read_ch(0);
            millivolts[i]=reading_to_mv(val);
            mean_mv+=millivolts[i];
        }
        mean_mv/=READINGS;
        printf("Mean Voltage:%.3f mV\n",mean_mv);
        printf("Variance:%.3f mV\n",variance(millivolts,READINGS,mean_mv));
        // sleep_ms(300);
    }
}
