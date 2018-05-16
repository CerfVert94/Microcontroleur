#include "mbed.h"
#include <math.h>

/**
Roqyun KO 
Uses ultrasonic range finder HC-SR04 
**/
const int pclk = 25e6; //Peripheral clock LPC1763

volatile double g_distance; //Global variable to store distance
volatile unsigned int temps = 0; // Time
volatile int flag = 0; // Edge trigger flag for ECHO signal
volatile int it_capture = 0; // Capture (timer) event flag.

const int I2C_RGB_ADDR = (0xC4);
const int I2C_LCD_ADDR = (0x7C);


//Communication 
Serial pc(USBTX,USBRX);
I2C i2c(I2C_SDA, I2C_SCL);

//Sensors / LEDs
AnalogIn ain(A0);
DigitalOut buzzer(A1);
DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);
DigitalOut led4(LED4);

//LCD
void init_LCD();
void setRGB(int r,int b, int g);
void Afficher(int ligne, const char *str);
void AfficherDistance(double distance);

//Sensor Events
float measureTemperature();
void onCaptureInterrupt();
void beeper();
void changeColor(double distance);

//Timer
void init_T2();
extern "C"  void TIMER2_IRQHandler();

Ticker clockclock;

void Counter();


void init_GPIO(){
	LPC_SC -> PCONP |= 1<<22;
	LPC_GPIO0->FIODIR0 |= (0x01);
	LPC_PINCON->PINSEL0 |= (3<<8); // P0.4 set capture 2.0
	LPC_GPIO0->FIOPIN &= ~(1<<4);
}


int main()
{
	
    init_LCD();
  //  Afficher(0, "Hello World!");
		init_GPIO();
		//init_T2();
		g_distance = 100.0;
		clockclock.attach(&Counter,1.0);
    while(1) {
				measureTemperature();
       // onCaptureInterrupt();
				//Counter(); //Mesure time in second.
        //setRGB(255*(ain),0,255*(1.0f - ain)); //Adjust the color through a potentiometer.

    }
}
void init_LCD()
{
    char cmd[2];

    cmd[0] = 0x00;
    cmd[1] = 0x3C;
    i2c.write(I2C_LCD_ADDR, cmd, 2);
    wait(0.1);

    cmd[0] = 0x00;
    cmd[1] = 0x0F;
    i2c.write(I2C_LCD_ADDR, cmd, 2);
    wait(0.1);

    cmd[0] = 0x00;
    cmd[1] = 0x01;
    i2c.write(I2C_LCD_ADDR, cmd, 2);
    wait(0.1);

    cmd[0] = 0x00;
    cmd[1] = 0x06;
    i2c.write(I2C_LCD_ADDR, cmd, 2);
    wait(0.1);
}

void Afficher(int ligne,const char *str)
{
    char cmd[2];
    int i = 0;

    cmd[0] = 0x00;
    if(ligne == 0)
        cmd[1] = 0x80;//0x80 = First line.
    else
        cmd[1] = 0xC0;// 0xC0 = Second line.
    i2c.write(I2C_LCD_ADDR, cmd, 2);

    while(str[i] != '\0') { // Write a character by character
        cmd[0] = 0x40;
        cmd[1] = str[i++];
        i2c.write(I2C_LCD_ADDR, cmd, 2);
        wait_us(5);
    }
}

void AfficherDistance(double distance)
{
		char str[30];
		char cmd[2];
		int i = 0;
	
		cmd[0] = 0x00;
		cmd[1] = 0x80;//0x80 = Premier ligne 0xC0 = Deuxieme ligne 
		i2c.write(I2C_LCD_ADDR, cmd, 2);
	
		sprintf(str,"Distance:%lf",distance);
		while(str[i] != '\0') {
			cmd[0] = 0x40;
			cmd[1] = str[i++];
			i2c.write(I2C_LCD_ADDR, cmd, 2);
			wait_us(50);
		}
		
}

void setRGB(int r,int b, int g)
{
    char cmd[2];
    cmd[0] = 0x00;
    cmd[1] = 0x00;
    i2c.write(I2C_RGB_ADDR, cmd, 2);

    cmd[0] = 0x01;
    cmd[1] = 0x00;
    i2c.write(I2C_RGB_ADDR, cmd, 2);

    wait(0.1);
    cmd[0] = 0x08;
    cmd[1] = 0xAA;
    i2c.write(I2C_RGB_ADDR, cmd, 2);
    wait(0.01);

    //Set red
    cmd[0] = 0x04;
    cmd[1] = r;
    i2c.write(I2C_RGB_ADDR, cmd, 2);
    wait(0.01);
    //Set green
    cmd[0] = 0x02;
    cmd[1] = g;
    i2c.write(I2C_RGB_ADDR, cmd, 2);
    wait(0.01);
    //Set blue
    cmd[0] = 0x03;
    cmd[1] = b;
    i2c.write(I2C_RGB_ADDR, cmd, 2);
    wait(0.01);
}


float measureTemperature()
{
    const int B = 4275;               // B value of the thermistor
    const double T0 = 298.15;
    double t = ain;
    char temp[128];
    float temperature = 0.0;

    temperature = 1.0 / (log(1.0 / (((float)ain) * 3.3 / 5.0)-1.0) / B+ 1.0 / T0)- 273.15; // convert to temperature via datasheet
    sprintf(temp,"temp = %.3lfC", temperature) ;
    Afficher(0,temp);
    wait_ms(1000);
    return temperature;
}


void init_T2(){
	int freq1 = 100e3; //10us 
	int freq2 = 5; // 200ms
	
	LPC_TIM2->MR0 = pclk/(freq1)  - 1; 
	LPC_TIM2->MR1 = pclk/(freq2)  - 1; 
	LPC_TIM2->MCR = 0x19; // Interrupt on MR0,MR1 and Reset on MR1.
	LPC_TIM2->CCR = 0x05; // //Detect RE(Rising Edge)
	
	NVIC_EnableIRQ(TIMER2_IRQn); 
	
	clockclock.attach(&beeper,0.1);
}


extern "C"  void TIMER2_IRQHandler()
{
	static unsigned int debut;
	
	if( (LPC_TIM2->IR &0x2) ==  0x2) // Interrupt MCR1
	{
			LPC_TIM2->IR = 0x2;
			LPC_GPIO0->FIOPIN0 = 0x1;
			
	}
	else if( (LPC_TIM2->IR & 0x1) ==  0x1) // Interrupt MCR0
	{
		LPC_TIM2->IR = 0x1;
		LPC_GPIO0->FIOPIN0 = 0x0;
		
	}
	else if( (LPC_TIM2->IR & (1 << 4)) != 0) // Interrupt CR0 (IN : ECHO signal)
	{
		LPC_TIM2->IR = (1 << 4);
		
		if (flag == 0) {
			flag = 1;
			debut = LPC_TIM2 -> CR0 ;
			LPC_TIM2->CCR = 0x06; //Detect FE(Falling Edge)
		}
		else {
			flag = 0;
			temps = LPC_TIM2 -> CR0 - debut; // The time gap between RE and FE.
			it_capture = 1; // Signal the end of distance measurment.
			LPC_TIM2->CCR = 0x05; //Detect RE(Rising Edge)
		}
	}
}
void Counter()
{
    char second[2];
    static int count = 0;

    sprintf(second,"%d",count);
    Afficher(1, second);

    if(count % 2 == 0) //Green if even.
        setRGB(0,255,0);
    else //Blue if odd.
        setRGB(0,0,255);
    count++;
    count %= 60;

   // wait(1);
}
void changeColor(double distance)
{
		if(distance < 20.0) {
				setRGB(255,0,0);
		}
		else if(distance < 40.0) {		
				setRGB(0,0,255);
		}
		else{
				setRGB(0,255,0);
		}
}

void beeper()
{
		if(g_distance < 20.0) {
				buzzer = 1;
		}
		else if(g_distance < 40.0) {		
				buzzer = !buzzer;
		}
		else{
				buzzer = 0.0f;
		}
}
void onCaptureInterrupt()
{
	float temperature;
	
	if(it_capture == 1) {
				temperature = measureTemperature();
				g_distance = ((double)temps / (double)pclk) / (58e-6);
				g_distance = g_distance + (0.6e2*temperature)*((double)temps / (double)pclk); // Taking account of temperature.
				AfficherDistance(g_distance);
				changeColor(g_distance);
				it_capture = 0;
		}
}
