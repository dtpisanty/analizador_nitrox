#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "displaylib_16/st7735.hpp"
#include <vector> // for error checking test
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
#define MAIN_LOOP_READINGS 33

// Mode button
#define BUTTON_GPIO 15
#define DEBOUNCE_MS 100
#define LONG_PRESS_MS 3000

//Physical constants
#define METERS_PER_ATM_SALT  10
#define METERS_PER_ATM_FRESH 11
#define O2_FRACTION_AIR      0.209f
// GLOBALS
ST7735_TFT myTFT;
void Setup(void);
float cal_mv;
int meters_per_atm;

typedef enum {
    BTN_IDLE,
    BTN_DEBOUNCING,
    BTN_HELD,
    BTN_LONG_TRIGGERED
} button_state_t;

static button_state_t btn_state = BTN_IDLE;
static absolute_time_t press_start_time;
static absolute_time_t debounce_start_time;

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
    return (int16_t)((raw[0]<<8)| raw[1]);
}

float reading_to_mv(uint16_t reading){
    return reading*0.007813f; //change to 0.007813f for o2 sensor
}

void Setup(void)
{
	stdio_init_all(); // optional for error messages , Initialize chosen serial port, default 38400 baud
	MILLISEC_DELAY(1000);
    ads1115_init();
	printf("TFT Start\r\n");
    meters_per_atm=METERS_PER_ATM_SALT;
	//*************** USER OPTION 0 SPI_SPEED + TYPE ***********
	bool bhardwareSPI = true; // true for hardware spi,
	if (bhardwareSPI == true)
	{								// hw spi
		uint32_t TFT_SCLK_FREQ = 8000; // Spi freq in KiloHertz , 1000 = 1Mhz
		myTFT.TFTInitSPIType(TFT_SCLK_FREQ, spi0);
	}
	else
	{								 // sw spi
		uint16_t SWSPICommDelay = 0; // optional SW SPI GPIO delay in uS
		myTFT.TFTInitSPIType(SWSPICommDelay);
	}
	//**********************************************************

	// ******** USER OPTION 1 GPIO *********
	// NOTE if using Hardware SPI clock and data pins will be tied to
	// the chosen interface eg Spi0 CLK=18 DIN=19)
	int8_t SDIN_TFT = 19;
	int8_t SCLK_TFT = 18;
	int8_t DC_TFT = 3;
	int8_t CS_TFT = 2;
	int8_t RST_TFT = 4;
	myTFT.setupGPIO(RST_TFT, DC_TFT, CS_TFT, SCLK_TFT, SDIN_TFT);
	//**********************************************************

	// ****** USER OPTION 2 Screen Setup ******
	uint8_t OFFSET_COL = 0;	   // 2, These offsets can be adjusted for any issues->
	uint8_t OFFSET_ROW = 0;	   // 3, with screen manufacture tolerance/defects
	uint16_t TFT_WIDTH = 128;  // Screen width in pixels
	uint16_t TFT_HEIGHT = 160; // Screen height in pixels
	myTFT.TFTInitScreenSize(OFFSET_COL, OFFSET_ROW, TFT_WIDTH, TFT_HEIGHT);
	// ******************************************

	// ******** USER OPTION 3 PCB_TYPE  **************************
	myTFT.TFTInitPCBType(myTFT.TFT_ST7735R_Red); // pass enum,4 choices,see README
	//**********************************************************
    gpio_init(BUTTON_GPIO);
    gpio_set_dir(BUTTON_GPIO, GPIO_IN);
    gpio_pull_up(BUTTON_GPIO);
}
float mv_to_fracO2(float mv){
    //%o2=mv*(3.92339494163424k)+0.68
    //return mv*79.1/(3.634146341*cal_mv)+0.68;
    return mv/cal_mv*.209;
}

float mod_at_ppo2(float fracO2, float target_ppo2){
    return meters_per_atm*(target_ppo2/fracO2-1);
}

void calibrate(){
    cal_mv=0;
    for(int i=0;i<READINGS;i++){
        uint16_t val=ads1115_read_ch(0);
        cal_mv+=reading_to_mv(val);
    }
    cal_mv/=READINGS;
}

void button_update() {
    bool pressed = !gpio_get(BUTTON_GPIO);  // active LOW (pulled up)

    switch (btn_state) {

        case BTN_IDLE:
            if (pressed) {
                debounce_start_time = get_absolute_time();
                btn_state = BTN_DEBOUNCING;
            }
            break;

        case BTN_DEBOUNCING:
            if (!pressed) {
                // bounced, false trigger
                btn_state = BTN_IDLE;
            } else if (absolute_time_diff_us(debounce_start_time, get_absolute_time()) > DEBOUNCE_MS * 1000) {
                // confirmed real press
                press_start_time = get_absolute_time();
                btn_state = BTN_HELD;
            }
            break;

        case BTN_HELD:
            if (!pressed) {
                // released before long-press threshold → short press action
                int64_t held_ms = absolute_time_diff_us(press_start_time, get_absolute_time()) / 1000;
                if (held_ms < LONG_PRESS_MS) {
                    if (meters_per_atm==METERS_PER_ATM_SALT){
                        meters_per_atm=METERS_PER_ATM_FRESH;
                    }
                    else{
                        meters_per_atm=METERS_PER_ATM_SALT;
                    }
                }
                btn_state = BTN_IDLE;
            } else if (absolute_time_diff_us(press_start_time, get_absolute_time()) > LONG_PRESS_MS * 1000) {
                // still held, threshold crossed → trigger calibration NOW
                calibrate();
                btn_state = BTN_LONG_TRIGGERED;
            }
            break;

        case BTN_LONG_TRIGGERED:
            // wait here until released, so calibration doesn't fire repeatedly
            if (!pressed) {
                btn_state = BTN_IDLE;
            }
            break;
    }
}


int main(){
    Setup();
    myTFT.fillScreen(myTFT.C_BLACK);
    myTFT.setCursor(5,40);
	myTFT.setFont(font_pico);
	myTFT.println("INICIANDO");
    sleep_ms(5000);
    calibrate();
    while (true) {
        button_update();
        float mean_mv=0;
        for(int i=0;i<MAIN_LOOP_READINGS;i++){
            uint16_t val=ads1115_read_ch(0);
            mean_mv+=val;
        }
        mean_mv/=MAIN_LOOP_READINGS;
        float frac_o2=mv_to_fracO2(mean_mv);
        if(frac_o2>1){frac_o2=1.0;}
        myTFT.fillScreen(myTFT.C_BLACK);
        myTFT.setCursor(5,10);
        myTFT.setTextColor(myTFT.C_GREEN, myTFT.C_BLACK);
        myTFT.setFont(font_arialBold);
        myTFT.print(frac_o2*100.0);//mean_mv*20.9/cal_mv);
        myTFT.println("%");
        myTFT.setCursor(5,40);
        myTFT.setTextColor(myTFT.C_YELLOW, myTFT.C_BLACK);
        myTFT.setFont(font_pico);
        char water_type = (meters_per_atm == METERS_PER_ATM_SALT) ? 'S' : 'F';
        char buf[20];
        snprintf(buf, sizeof(buf), "MOD 1.4 (M%cW)", water_type);
        myTFT.println(buf);
        myTFT.setFont(font_arialBold);
        myTFT.print(mod_at_ppo2(frac_o2,1.4));//mean_mv*20.9/cal_mv);
        myTFT.print("m");
        myTFT.setCursor(5,70);
        myTFT.setTextColor(myTFT.C_YELLOW, myTFT.C_BLACK);
        myTFT.setFont(font_pico);
        snprintf(buf, sizeof(buf), "MOD 1.6 (M%cW)", water_type);
        myTFT.setFont(font_arialBold);
        myTFT.print(mod_at_ppo2(frac_o2,1.6));//mean_mv*20.9/cal_mv);
        myTFT.print("m");
        printf("Mean Voltage:%.3f mV\n",mean_mv);
    }
}
