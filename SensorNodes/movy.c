// PIR motion sensor connected to D3 (INT1)
// When RISE happens on D3, the sketch transmits a "MOTION" msg to receiver Moteino and goes back to sleep
// In sleep mode, Moteino + PIR motion sensor use about ~78uA

#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <LowPower.h> //get library from: https://github.com/lowpowerlab/lowpower
                      //writeup here: http://www.rocketscream.com/blog/2011/07/04/lightweight-low-power-arduino-library/

//*********************************************************************************************
// *********** IMPORTANT SETTINGS - YOU MUST CHANGE/ONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NODEID        13    //unique for each node on same network
#define NETWORKID     101  //the same on all nodes that talk to each other
#define GATEWAYID     1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "thisIsEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Remove/comment if you have RFM69W!
#define ACK_TIME      30 // max # of ms to wait for an ack
#define ONBOARDLED     9  // Moteinos have LEDs on D9
#define BATT_MONITOR  A7  // Sense VBAT_COND signal (when powered externally should read ~3.25v/3.3v (1000-1023), when external power is cutoff it should start reading around 2.85v/3.3v * 1023 ~= 880 (ratio given by 10k+4.7K divider from VBAT_COND = 1.47 multiplier)
#define BATT_CYCLES   30  //read and report battery voltage every this many wakeup cycles (ex 30cycles * 8sec sleep = 240sec/4min)
#define MOTIONPIN      3 //hardware interrupt 1 (D3) WDT ARDUINO
#define SERIAL_BAUD    115200

#define SERIAL_EN             //comment this out when deploying to an installed SM to save a few KB of sketch size
#ifdef SERIAL_EN
#define DEBUG(input)   {Serial.print(input); delay(1);}
#define DEBUGln(input) {Serial.println(input); delay(1);}
#else
#define DEBUG(input);
#define DEBUGln(input);
#endif

//struct for wireless data transmission
typedef struct {    
  int       nodeID;     
  int       deviceID;   
  unsigned long   uptime;     
  float     var2_float;     // 1 for motion detected
  float     var3_float;     //battery status
} Payload;
Payload theData;

RFM69 radio;
volatile boolean motionDetected=false;
float batteryVolts = 5;

void setup() {
  Serial.begin(SERIAL_BAUD);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  pinMode(MOTIONPIN, INPUT);
  attachInterrupt( 1 , motionIRQ, RISING);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  DEBUGln(buff);
  pinMode(ONBOARDLED, OUTPUT);
  pinMode(BATT_MONITOR, INPUT);
}

void motionIRQ()
{
  motionDetected=true;
}

byte batteryReportCycles=0;
void loop() {
  checkBattery();
  if (motionDetected) {
    
    theData.var2_float = 1;
    theData.var3_float = batteryVolts;

    if (radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData))) {
      DEBUG("MOTION ACK:OK! RSSI:");
      DEBUG(radio.RSSI);
      batteryReportCycles = 0;
    } else {
      DEBUG("MOTION ACK:NOK...");
    }
    DEBUG(" VIN: ");
    DEBUGln(batteryVolts);

    radio.sleep();
  } else if (batteryReportCycles == BATT_CYCLES) {
    theData.var2_float = 0;
    theData.var3_float = batteryVolts;
    radio.send(GATEWAYID, (const void*)(&theData), sizeof(theData));
    radio.sleep();
    batteryReportCycles=0;
  }
  motionDetected=false; //do NOT move this after the SLEEP line below or motion will never be detected
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  batteryReportCycles++;
}

byte cycleCount=BATT_CYCLES;
void checkBattery() {
  if (cycleCount++ == BATT_CYCLES) //only read battery every BATT_CYCLES sleep cycles
  {
    unsigned int readings=0;
    for (byte i=0; i<10; i++) //take 10 samples, and average
      readings+=analogRead(BATT_MONITOR);
    batteryVolts = (readings / 10.0) * 0.00322 * 1.47;
    cycleCount = 0;
  }
}