#include <RFM69.h>
#include <SPI.h>

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

#define GASPIN A0
#define REDPIN  6
#define PIEZOPIN  5

//struct for wireless data transmission
typedef struct {    
  int       nodeID;     
  int       deviceID;   
  unsigned long   uptime;     
  float     var2_float;     
  float     var3_float;   
} Payload;
Payload theData;

int TRANSMITPERIOD = 300; //transmit a packet to gateway so often (in ms)
RFM69 radio;
long lastPeriod = -1;
//end RFM69 ------------------------------------------


void setup() {
  Serial.begin(SERIAL_BAUD);          //  setup serial
  Serial.println("starting");
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
#ifdef IS_RFM69HW
  radio.setHighPower(); //uncomment only for RFM69HW!
#endif
  radio.encrypt(ENCRYPTKEY);
  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);
  theData.nodeID = NODEID;  //this node id should be the same for all devices in this node 
  pinMode(REDPIN, OUTPUT);
  pinMode(PIEZOPIN, OUTPUT); 
}


void loop() {  
  int gasMonitor = analogRead(GASPIN);
  int currPeriod = millis()/TRANSMITPERIOD;
  if (currPeriod != lastPeriod) {
    Serial.print("\ngasMonitor=");
    Serial.println(gasMonitor);

    //send data
    theData.deviceID = 6;
    theData.uptime = millis();
    theData.var2_float = gasMonitor;
    theData.var3_float = 0;

    Serial.print("Sending struct (");
    Serial.print(sizeof(theData));
    Serial.print(" bytes) ... ");
    if (radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData))) {
      Serial.print(" ok!");
    } else {
      Serial.print(" nothing...");
    }
    lastPeriod=currPeriod;
  }
  while(gasMonitor > 100){
      theData.var3_float = 1;                                                     //Alarm signal
      radio.sendWithRetry(GATEWAYID, (const void*)(&theData), sizeof(theData));
      Serial.println("ALARM!! sending alarm");
      playTone(100, 2600, PIEZOPIN);
      analogWrite(REDPIN, 255);
      delay(100);
      digitalWrite(REDPIN, LOW);
      gasMonitor = analogRead(GASPIN);
    }
  analogWrite(PIEZOPIN, 0);
  delay(1000);
}

void playTone(long duration, int freq, int piezoPin) {
    duration *= 1000;
    int period = (1.0 / freq) * 1000000;
    long elapsed_time = 0;
    while (elapsed_time < duration) {
        digitalWrite(piezoPin,HIGH);
        delayMicroseconds(period / 2);
        digitalWrite(piezoPin, LOW);
        delayMicroseconds(period / 2);
        elapsed_time += (period);
    }
}