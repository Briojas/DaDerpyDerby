#include <Arduino.h>

//Time management
#include <time.h>
  //need absolute time for syncing across devices
const char timeServer1[] = "pool.ntp.org"; 
const char timeServer2[] = "time.nist.gov"; 
const char timeServer3[] = "time.google.com"; 

//WiFi
#include <wifiSetup.h>
#include <wifiLogin.h>
//TODO: Manage secure vs not-secure wifi_client selection based on MQTT port selection
// WiFiClient wifi_client;
WiFiClientSecure wifi_client;

//MQTT
#include <mqttSetup.h>
#include <mqttLogin.h>
MQTTClient mqtt_client;
const int port = 8883;
const char clientName[] = "derby_kart"; //must be >= 8 chars
const int numPubs = 1;
mqtt_pubSubDef_t pubs[numPubs];
const int numSubs = 4;
mqtt_pubSubDef_t subs[numSubs];
void readSubs(String &topic, String &payload){
    Serial.println("incoming: " + topic + " - " + payload);
    for(int i=0; i < numSubs; i++){
        if(topic == subs[i].topic){ //check each message in the array for the correct subscriber
            subs[i].payload = payload; //store payload in the correct subscriber message
            break;
        }
    }
}

//General inits and defs
MQTT_Client_Handler kart_mqtt_client(mqtt_client, wifi_client, brokerName, subs, numSubs, readSubs, port); //initialize the mqtt handler
String getTimestamp();
  //PWM settings
const int frequency = 30000;
const int resolution = 8;
  //Initialize motors
void setMotorPins(int motor[4]);
  //Kart control
void setMotorPow();
bool powAtDirForDur(double power, String dir, double dur, double startTime);
  //Motor command options
void stopAll();                   //dir = "STP"
void forwardDrive(double power);  //dir = "FOR" 
void reverseDrive(double power);  //dir = "REV" 
void rotateCW(double power);      //dir = "RCW" 
void rotateCCW(double power);     //dir = "CCW" 
void strafeLeft(double power);    //dir = "STL"
void strafeRight(double power);   //dir = "STR"
  //Motor dependencies
double mapPowerToDuty(double percentage);
void stopMotor(int motor[4]);
void forwardMotor(int motor[4], double power);
void reverseMotor(int motor[4], double power);
  //Motor defs
    //int motor[4] = {PWM_channel, PWM_pin, forward_pin, reverse_pin}
int FrontLeft[4] = {0, 27, 14, 18};
int FrontRight[4] = {1, 13, 5, 12};
int BackRight[4] = {2, 33, 21, 25};
int BackLeft[4] = {3, 4, 2, 19};
int* wheels[4] = {FrontRight, FrontLeft, BackLeft, BackRight};
int wheelPower[4]; //fr, fl, bl, br

void setup() {
  Serial.begin(115200);
///////////////   WiFi   ///////////////
  wifi_init(wifiName, wifiPW);
  wifi_client.setInsecure(); //TODO: only call when WiFiClientSecure used
///////////////   MQTT   ///////////////
  String deviceName = clientName; //converting const char to str
                //$$ SUBS $$//      (be sure to update numSubs above when defining new ones)
    //listening to broker status
      //front right motor
  subs[0].topic = "/" + deviceName + "/fr"; 
  subs[0].qos = 0;
      //front left motor
  subs[1].topic = "/" + deviceName + "/fl"; 
  subs[1].qos = 0;
      //back left motor
  subs[2].topic = "/" + deviceName + "/bl"; 
  subs[2].qos = 0;
      //back right motor
  subs[3].topic = "/" + deviceName + "/br"; 
  subs[3].qos = 0;
                //$$ PUBS $$//      (be sure to update numPubs above when defining new ones)
  pubs[0].topic = "/" + deviceName + "/data"; //currently not publishing anythiing
  pubs[0].qos = 2; 
  pubs[0].retained = true;
                //$$ connect $$//
  kart_mqtt_client.connect(clientName, brokerLogin, brokerPW);
///////////////   Time   ///////////////
  configTime(-5 * 3600, 0, timeServer1, timeServer2, timeServer3);
///////////////   Iinit Motors   ///////////////
  setMotorPins(FrontLeft);
  setMotorPins(FrontRight);
  setMotorPins(BackRight);
  setMotorPins(BackLeft);
}

void loop() {
  if(!kart_mqtt_client.loop()){
    stopAll();
    kart_mqtt_client.connect(clientName, brokerLogin, brokerPW);
  }
  setMotorPow();
}

String getTimestamp(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "0000-00-00T-00:00:00-00:00";
  }

  char timeYear[5];
  strftime(timeYear, 5, "%Y", &timeinfo);
  char timeMonth[3];
  strftime(timeMonth, 5, "%m", &timeinfo);
  char timeDay[3];
  strftime(timeDay, 3, "%d", &timeinfo);
  char timeHr[3];
  strftime(timeHr, 3, "%H", &timeinfo);
  char timeMin[3];
  strftime(timeMin, 3, "%M", &timeinfo);
  char timeSec[3];
  strftime(timeSec, 3, "%S", &timeinfo);

  String timestamp;
  timestamp.concat(timeYear);
  timestamp.concat("-");
  timestamp.concat(timeMonth);
  timestamp.concat("-");
  timestamp.concat(timeDay);
  timestamp.concat("T-");
  timestamp.concat(timeHr);
  timestamp.concat(":");
  timestamp.concat(timeMin);
  timestamp.concat(":");
  timestamp.concat(timeSec);
  timestamp.concat("-");
  timestamp.concat("05:00"); //cst offset
  return timestamp;
}

void setMotorPow(){
  for (int i = 0; i < 4; i++){
    if(wheelPower[i] != subs[i].payload.toInt()){
      wheelPower[i] = subs[i].payload.toInt();
      if(wheelPower[i] == 0){
        stopMotor(wheels[i]);
      }else if(wheelPower[i] < 0){
        reverseMotor(wheels[i], wheelPower[i]);
      }else if(wheelPower[i] > 0){
        forwardMotor(wheels[i], wheelPower[i]);
      }
    }
  }
}

bool powAtDirForDur(double power, String dir, double dur, double startTime){
  double currTime = millis()/1000;
  if((currTime - startTime) < dur){
    if(dir == "FOR"){
      forwardDrive(power);
    }else if(dir == "REV"){
      reverseDrive(power);
    }else if(dir == "RCW"){
      rotateCW(power);
    }else if(dir == "CCW"){
      rotateCCW(power);
    }else if(dir == "STL"){
      strafeLeft(power);
    }else if(dir == "STR"){
      strafeRight(power);
    }else{
      stopAll();
    }
    return true;
  }else{
    stopAll();
    delay(25);
    return false;
  }
}

void stopAll(){
  stopMotor(FrontLeft);
  stopMotor(FrontRight);
  stopMotor(BackRight);
  stopMotor(BackLeft);
}

void forwardDrive(double power){  //dir = "FOR"
  forwardMotor(FrontLeft, power);
  forwardMotor(FrontRight, power);
  forwardMotor(BackRight, power);
  forwardMotor(BackLeft, power);
}

void reverseDrive(double power){ //dir = "REV"
  reverseMotor(FrontLeft, power);
  reverseMotor(FrontRight, power);
  reverseMotor(BackRight, power);
  reverseMotor(BackLeft, power);
}

void rotateCW(double power){ //dir = "RCW"
  forwardMotor(FrontLeft, power);
  reverseMotor(FrontRight, power);
  reverseMotor(BackRight, power);
  forwardMotor(BackLeft, power);
}

void rotateCCW(double power){//dir = "CCW" 
  reverseMotor(FrontLeft, power);
  forwardMotor(FrontRight, power);
  forwardMotor(BackRight, power);
  reverseMotor(BackLeft, power);
}

void strafeLeft(double power){//dir = "STL"
  reverseMotor(FrontLeft, power);
  forwardMotor(FrontRight, power);
  reverseMotor(BackRight, power);
  forwardMotor(BackLeft, power);
}

void strafeRight(double power){//dir = "STR"
  forwardMotor(FrontLeft, power);
  reverseMotor(FrontRight, power);
  forwardMotor(BackRight, power);
  reverseMotor(BackLeft, power);
}

void setMotorPins(int motor[4]){
  ledcSetup(motor[0], frequency, resolution);
  for (int i = 1; i < 4; i++){
    pinMode(motor[i], OUTPUT);
  }
  ledcAttachPin(motor[1], motor[0]);
}

double mapPowerToDuty(double percentage){
  double duty; //256-bit
  double minPower = 62; //62% of 255 is min operating power for sLINKy
  double maxPower = 77; //77% of 255 is max operating power for sLINKy
  if (percentage <= 0){
    duty = 255 * (minPower / 100);
  }else if (percentage >= 100){
    duty = 255 * (maxPower / 100);
  }else {
    duty = 255 * ((((maxPower - minPower)/ 100))*percentage + minPower)/100;
  }
  return duty;
}

void stopMotor(int motor[4]){
  digitalWrite(motor[2], LOW); 
  digitalWrite(motor[3], LOW);
  ledcWrite(motor[0], 0);
}

void forwardMotor(int motor[4], double power){
  digitalWrite(motor[2], HIGH);
  digitalWrite(motor[3], LOW);
  ledcWrite(motor[0], mapPowerToDuty(power));
}

void reverseMotor(int motor[4], double power){
  digitalWrite(motor[3], HIGH);
  digitalWrite(motor[2], LOW);
  ledcWrite(motor[0], mapPowerToDuty(power));
}

  //for debugging
// void testMotor(int motor[4]){
//   Serial.print("Testing motor running on PWM channel: ");
//   Serial.println(motor[0]);
//   forwardMotor(motor, 70);
//   Serial.println("Motor driving forward at 70% for 2s");
//   delay(2000);
//   stopMotor(motor);
//   Serial.println("Motor stopped for 1s");
//   delay(1000);
//   reverseMotor(motor, 70);
//   Serial.println("Motor driving reverse at 70% for 2s");
//   delay(2000);
//   stopMotor(motor);
//   Serial.println("Motor stopped. Testing finished.");
//   delay(1000);
// }



