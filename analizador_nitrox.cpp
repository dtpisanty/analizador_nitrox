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

// GLOBALS
ST7735_TFT myTFT;
void Setup(void);
void DisplayReset(void);
float cal_mv;
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

void Setup(void)
{
	stdio_init_all(); // optional for error messages , Initialize chosen serial port, default 38400 baud
	MILLISEC_DELAY(1000);
    ads1115_init();
	printf("TFT Start\r\n");

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
}

float variance(float *list, int length,float mean){
    float sq_diff=0;
    for(int i=0;i<length;i++){
        sq_diff+=(list[i]-mean)*(list[i]-mean);
    }
    return sq_diff/length;
}
float mv_to_fracO2(float mv){
    //%o2=mv*(3.92339494163424k)+0.68
    //return mv*79.1/(3.634146341*cal_mv)+0.68;
    return mv/cal_mv*.209;
}

float mod_14(float fracO2){
    return 10*(1.4/fracO2-1);
}
int main()
{
    Setup();
    myTFT.fillScreen(myTFT.C_BLACK);
    myTFT.setCursor(5,40);
	myTFT.setFont(font_pico);
	myTFT.println("INICIANDO");
    sleep_ms(5000);
    cal_mv=0;
    for(int i=0;i<READINGS;i++){
        uint16_t val=ads1115_read_ch(0);
        cal_mv+=reading_to_mv(val);
    }
    cal_mv/=READINGS;
    while (true) {
        float millivolts[READINGS];
        float mean_mv=0;
        for(int i=0;i<READINGS;i++){
            uint16_t val=ads1115_read_ch(0);
            millivolts[i]=reading_to_mv(val);
            mean_mv+=millivolts[i];
        }
        mean_mv/=READINGS;
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
        myTFT.println("MOD 1.4 (MSW)");
        myTFT.setFont(font_arialBold);
        myTFT.print(mod_14(frac_o2));//mean_mv*20.9/cal_mv);
        myTFT.print("m");
        printf("Mean Voltage:%.3f mV\n",mean_mv);
        printf("Variance:%.3f mV\n",variance(millivolts,READINGS,mean_mv));
        // sleep_ms(300);
    }
}
