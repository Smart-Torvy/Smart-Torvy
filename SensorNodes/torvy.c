#include <RFM69.h>
#include <SPI.h>
#include <LowPower.h>
#include <DHT.h>            //Library for DHT 

#define NODEID        13    //unique for each node on same network
#define NETWORKID     101  //the same on all nodes that talk to each other
#define GATEWAYID     1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY   RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "thisIsEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ACK_TIME      30 // max # of ms to wait for an ack
#define LED           9  // Moteinos have LEDs on D9
#define SERIAL_BAUD   115200  //must be 9600 for GPS, use whatever if no GPS
#define BATT_MONITOR  A7  // Sense VBAT_COND signal (when powered externally should read ~3.25v/3.3v (1000-1023), when external power is cutoff it should start reading around 2.85v/3.3v * 1023 ~= 880 (ratio given by 10k+4.7K divider from VBAT_COND = 1.47 multiplier)
#define BATT_CYCLES   30  //read and report battery voltage every this many wakeup cycles (ex 30cycles * 8sec sleep = 240sec/4min)
// Uncomment whatever type you're using:
//#define DHTTYPE DHT11                     // DHT 11 
#define DHTTYPE DHT22                       // DHT 22  (AM2302)
//#define DHTTYPE DHT21                     // DHT 21 (AM2301)
#define DHTPIN 2
#define REDPIN 9
#define BLUEPIN 10
#define GREENPIN 11
#define BUTTONPIN 3 
#define BUTTONPIN_IRQ 1
#define RELAYSET 13
#define RELAYRESET 12
#define TORVY_1_DEVICEID 3
#define TORVY_2_DEVICEID 4


DHT dht(DHTPIN, DHTTYPE);

//struct for wireless data transmission
typedef struct {    
  int       nodeID;     
  int       deviceID;   
  unsigned long   uptime;     
  float     var2_float;     // on/off button for thermostath
  float     var3_float;   
} Payload;
Payload theData;

int TRANSMITPERIOD = 300; //transmit a packet to gateway so often (in ms)
RFM69 radio;
long lastPeriod = -1;
volatile boolean buttonPressed = false;
float batteryVolts = 5;
byte batteryReportCycles=0;

//end RFM69 ------------------------------------------

void setup() {
  Serial.begin(SERIAL_BAUD);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  theData.nodeID = NODEID;  
  pinMode(BUTTONPIN, INPUT);
  attachInterrupt(BUTTONPIN_IRQ, pressIRQ, RISING);
  pinMode(REDPIN, OUTPUT);
  pinMode(BLUEPIN, OUTPUT);
  pinMode(GREENPIN, OUTPUT);
  pinMode(RELAYRESET, OUTPUT);
  pinMode(RELAYSET, OUTPUT);
  pinMode(BATT_MONITOR, INPUT);
  dht.begin(); 
}

void pressIRQ(void){
  buttonPressed = true;
}

void loop() {  
  checkBattery();
  if(buttonPressed) {
    theData.var2_float = 1;
  }
  if(digitalRead(BUTTONPIN) == HIGH){
    theData.var2_float = 0;
  }
  //check for any received packets
  if (radio.receiveDone()) {
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    Serial.print("   [RX_RSSI:");Serial.print(radio.readRSSI());Serial.print("]");
    if (radio.DATALEN == sizeof(Payload)) {
      theData = *(Payload*)radio.DATA; //assume radio.DATA actually contains our struct and not something else
      Serial.print("Received Device ID = ");
      Serial.println(theData.deviceID);  
      Serial.print ("    UpTime=");
      Serial.println (theData.uptime);
      Serial.print ("    var2_float=");
      Serial.println (theData.var2_float);
      Serial.print ("    var3_float=");
      Serial.println (theData.var3_float);
    }
    else {
      Serial.print("Invalid data ");
    }
    if (radio.ACKRequested()) {
      radio.sendACK();
      Serial.println(" - ACK sent");
    }
  }     //End receiving part
  if( theData.var2_float == 1){
      digitalWrite(RELAYSET, HIGH);
      delay(1);
      digitalWrite(RELAYSET, LOW);
      digitalWrite(GREENPIN, HIGH);   //TODO adding fadeing effect (breathing)
  } else if (theData.var2_float == 0) {
      digitalWrite(RELAYRESET, HIGH);
      delay(1);
      digitalWrite(RELAYRESET, LOW);
      digitalWrite(GREENPIN, LOW);
      // TODO mandare temperatura e voltaggio batteria su theData.deviceID diversi;
      radio.send(GATEWAYID, (const void*)(&theData), sizeof(theData));
      radio.sleep();
      batteryReportCycles=0;
      buttonPressed = false;
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
  batteryReportCycles++;
  int currPeriod = millis()/TRANSMITPERIOD;

    
  //Sending part
  if (currPeriod != lastPeriod) {

    float h = dht.readHumidity();
    // Read temperature as Celsius
    float t = dht.readTemperature();
    // Read temperature as Farenheit
    float f = dht.readTemperature(true);
    float hi = dht.computeHeatIndex(f, h);
    int thi = (hi - 32)/  1.8;

    Serial.print("Humidity=");
    Serial.print(h);
    Serial.print("   Temp=");
    Serial.print(t);
    Serial.print("*C");
    Serial.print("   THI=");
    Serial.print(thi);
    Serial.print("*C\n");

    //send data
    theData.deviceID = 6;
    theData.uptime = millis();
    theData.var2_float = t;
    theData.var3_float = h;
    
    Serial.print("Sending struct (");
    Serial.print(sizeof(theData));
    Serial.print(" bytes) ... ");
    if (radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData)))
      Serial.print(" ok!");
    else Serial.println(" nothing...");
    lastPeriod=currPeriod;
    delay(100);
  }   //end sending part
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