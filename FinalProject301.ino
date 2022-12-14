//Macros
#define RDA 0x80
#define TBE 0x20

//Headers
#include <LiquidCrystal.h>
#include <DHT.h>
#include <uRTCLib.h>
#include <Stepper.h>

// UART Pointers
volatile unsigned char *myUCSR0A  = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B  = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C  = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0   = (unsigned int *)0x00C4;
volatile unsigned char *myUDR0    = (unsigned char *)0x00C6;

//Analog Pointers
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

//PortA Pointers
volatile unsigned char* port_a = (unsigned char*) 0x22; 
volatile unsigned char* ddr_a  = (unsigned char*) 0x21; 
volatile unsigned char* pin_a  = (unsigned char*) 0x20; 

//PortB Pointers
volatile unsigned char* port_b = (unsigned char*) 0x25; 
volatile unsigned char* ddr_b  = (unsigned char*) 0x24; 
volatile unsigned char* pin_b  = (unsigned char*) 0x23;

//PortE Pointers
volatile unsigned char* port_e = (unsigned char*) 0x2E; 
volatile unsigned char* ddr_e  = (unsigned char*) 0x2D; 
volatile unsigned char* pin_e  = (unsigned char*) 0x2C;

//Port H Pointers
volatile unsigned char* port_h = (unsigned char*) 0x102; 
volatile unsigned char* ddr_h  = (unsigned char*) 0x101; 
volatile unsigned char* pin_h  = (unsigned char*) 0x100;

//Port L Pointers
volatile unsigned char* port_l = (unsigned char*) 0x10B; 
volatile unsigned char* ddr_l  = (unsigned char*) 0x10A; 
volatile unsigned char* pin_l  = (unsigned char*) 0x109;

//LCD
LiquidCrystal lcd(30, 31, 5, 4, 3, 32);

//DHT
#define DHTPIN A0
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

//RTC
uRTCLib rtc(0x68);

//Stepper
const int stepsPerRevolution = 2038;
Stepper myStepper = Stepper(stepsPerRevolution, 7, 9, 8, 10);

//state variables
int water_lvl;
int w_thresh = 400;
float temp; 
float hu;
int t_thresh = 73;
bool d_flag;
float loops = 60;

//RTC variables
int hour;
int minute;
int sec;

//button variables
int lastButtonState;    // the previous state of button
int currentButtonState; // the current state of button

void setup() {
  // Initialize Serial
  U0init(9600);

  //Initialize ADC 
  adc_init();

  //Initialize Disable Button
  //set PA0 as input and enable pullup resistor on PA0
  *ddr_a &= 0b11111110;
  *port_a |= 0b00000001;

  //Initialize Stepper Buttons
    //set PA1 as input and enable pullup resistor on PA0
  *ddr_a &= 0b11111101;
  *port_a |= 0b00000010;
      //set PA3 as input and enable pullup resistor on PA0
  *ddr_a &= 0b11110111;
  *port_a |= 0b00001000;

  //Initialize LED pins
  set_PB_as_output(5);
  set_PB_as_output(6);
  set_PE_as_output(4);
  set_PH_as_output(3);

  //Initialize Fan Pin
  set_PL_as_output(7);

  //Initialize LCD
  set_PB_as_output(7);
  lcd.begin(16, 2);

  //Initialize DHT
  dht.begin();

  //Initialize RTC
  URTCLIB_WIRE.begin();
  
}

void loop() {

  //track toggle
  lastButtonState    = currentButtonState;      // save the last state
  currentButtonState = ReadPinA0(); // read new state

  //track time
  rtc.refresh();

  //track water level
  water_lvl = adc_read(5);

  //track temperature and humidity every minute
  delay(250);
  if(loops == 60){
    temp = dht.readTemperature(true);
    hu = dht.readHumidity();
    loops = 0;
  }  
  loops = loops +.25;
  lcd.clear();
  
  //Enabled States
  if(currentButtonState == HIGH){
    //Disable Yellow LED & Fan
    *port_b &= 0b11011111;
    *port_l &= 0b01111111;
    if(water_lvl > w_thresh && temp <= t_thresh){Idle();}
    if(water_lvl > w_thresh && temp > t_thresh){Running();}
    if(water_lvl <= w_thresh){Error();}
  }

  //Disabled State
  if(currentButtonState == LOW){Disabled();}
}

void Idle(){
//green 1, red 0, blue 0
*port_b |= 0b01000000;
*port_e &= 0b11101111;
*port_h &= 0b11110111;

//LCD Display Temp&Humidity
lcd.setCursor(0, 0);
lcd.print("Hmdty");
lcd.print(hu);
lcd.print(" %\t");
lcd.print(loops);
lcd.print("s");
lcd.setCursor(0,1);
lcd.print("Temp: "); 
lcd.print(temp);
lcd.println(" *F\t");
}

void Error(){
//green 0, red 1, blue 0
*port_b &= 0b10111111;
*port_e |= 0b00010000;
*port_h &= 0b11110111;

//LCD Display ERROR
lcd.setCursor(0, 0);
lcd.print("ERROR");
}

void Running(){
//green 0, red 0, blue 1
*port_b &= 0b10111111;
*port_e &= 0b11101111;
*port_h |= 0b00001000;

//Enable Motor
*port_l |= 0b10000000;

//LCD Display Temp&Humidity
lcd.setCursor(0, 0);
lcd.print("Hmdty");
lcd.print(hu);
lcd.print(" %\t");
lcd.setCursor(0,1);
lcd.print(loops);
lcd.print("s");
lcd.print("Temp: "); 
lcd.print(temp);
lcd.println(" *F\t");
}

bool Disabled(){
//Disable Fan
*port_l &= 0b01111111;

//yellow 1, green 0, red 0, blue 0
*port_b |= 0b00100000;
*port_b &= 0b10111111;
*port_e &= 0b11101111;
*port_h &= 0b11110111;
lcd.clear();
loops = 0;

  if(ReadPinA1() == LOW){
  myStepper.setSpeed(10);
	myStepper.step(stepsPerRevolution);
  }

  else if(ReadPinA3() == LOW){
  myStepper.setSpeed(10);
	myStepper.step(-stepsPerRevolution);
  }
}

void U0init(unsigned long U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}

void adc_init()
{
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 5 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 3 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111;  // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111;// clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000;// clear bit 4-0 to 0 to reset the channel and gain bits
}

unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if(adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
}

int ReadPinA0(){
 // if the pin is high
  if(*pin_a & 0b00000001)
  {
    return HIGH;
  }
  // if the pin is low
  else
  {
    return LOW;
  }
}

int ReadPinA1(){
 // if the pin is high
  if(*pin_a & 0b00000010)
  {
    return HIGH;
  }
  // if the pin is low
  else
  {
    return LOW;
  }
}

int ReadPinA3(){
 // if the pin is high
  if(*pin_a & 0b00001000)
  {
    return HIGH;
  }
  // if the pin is low
  else
  {
    return LOW;
  }
}

void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A&TBE)==0){}; //wait for TBE HIGH
  *myUDR0 = U0pdata; 
}

void set_PB_as_output(unsigned char pin_num)
{
    *ddr_b |= 0x01 << pin_num;
}

void set_PE_as_output(unsigned char pin_num)
{
    *ddr_e |= 0x01 << pin_num;
}

void set_PH_as_output(unsigned char pin_num)
{
    *ddr_h |= 0x01 << pin_num;
}

void set_PL_as_output(unsigned char pin_num)
{
    *ddr_l |= 0x01 << pin_num;
}
