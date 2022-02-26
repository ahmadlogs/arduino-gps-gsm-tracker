#include <SoftwareSerial.h>
#include <AltSoftSerial.h>
#include <TinyGPS++.h>

//sender phone number with country code
const String PHONE = "enter_your_phone";

//use to turn bike ON and OFF
#define ignition_switch 12 
#define ignition_sensor A5

//GSM Module RX pin to Arduino 3
//GSM Module TX pin to Arduino 2
#define rxPin 2
#define txPin 3
SoftwareSerial sim800(rxPin,txPin);

//GPS Module RX pin to Arduino 9
//GPS Module TX pin to Arduino 8
AltSoftSerial neogps;

TinyGPSPlus gps;

String sms_status,sender_number,received_date,msg;
boolean ignition_status = false;
boolean tracking_status = false;
boolean reply_status = true; //not completely implemented
boolean anti_theft = false;



unsigned long previousMillis = 0; // last time update
long interval = 60000; // interval at which to do something (milliseconds)

//***************************************************
void setup() {
  delay(7000);
  Serial.begin(115200);
  //Serial.println("Arduino serial initialize");
  
  sim800.begin(9600);
  neogps.begin(9600);
  //Serial.println("neogps Software serial initialize");
  //delay(1000);

  pinMode(ignition_switch, OUTPUT);
  pinMode(ignition_sensor, INPUT);

  sms_status = "";
  sender_number="";
  received_date="";
  msg="";

  sim800.println("AT"); //Check GSM Module
  delay(1000);
  sim800.println("ATE1"); //Echo ON
  delay(1000);
  sim800.println("AT+CPIN?"); //Check SIM ready
  delay(1000);
  sim800.println("AT+CMGF=1"); //SMS text mode
  delay(1000);
  sim800.println("AT+CNMI=2,2,0,0,0"); //set sms received format
  delay(1000);
  //sendATcommand("AT+CSCLK=2", "OK", 1000);

}
//***************************************************

void loop() {
  /////////////////////////////////////////////////////
  ignition_status = getIgnitionStatus();
  /////////////////////////////////////////////////////
  if(tracking_status == true && ignition_status == true){
    unsigned long currentMillis = millis();
    if(currentMillis - previousMillis > interval) {
       previousMillis = currentMillis;
       sendGpsToServer();
    }
  }
  /////////////////////////////////////////////////////
  if(anti_theft == true && ignition_status == true){
    digitalWrite(ignition_switch, HIGH);
  }
  /////////////////////////////////////////////////////
  while(sim800.available()){
    parseData(sim800.readString());
  }
  /////////////////////////////////////////////////////
  while(Serial.available())  {
    sim800.println(Serial.readString());
  }
  /////////////////////////////////////////////////////

} //main loop ends

//***************************************************
void parseData(String buff){
  Serial.println(buff);

  unsigned int len, index;
  //////////////////////////////////////////////////
  //Remove sent "AT Command" from the response string.
  index = buff.indexOf("\r");
  buff.remove(0, index+2);
  buff.trim();
  //////////////////////////////////////////////////
  
  //////////////////////////////////////////////////
  if(buff != "OK"){
    index = buff.indexOf(":");
    String cmd = buff.substring(0, index);
    cmd.trim();
    
    buff.remove(0, index+2);
    //Serial.println(buff);
    
    if(cmd == "+CMTI"){
      //get newly arrived memory location and store it in temp
      //temp = 4
      index = buff.indexOf(",");
      String temp = buff.substring(index+1, buff.length()); 
      temp = "AT+CMGR=" + temp + "\r"; 
      //AT+CMGR=4 i.e. get message stored at memory location 4
      sim800.println(temp); 
    }
    else if(cmd == "+CMGR"){
      extractSms(buff);
      //Serial.println("extractSms Success.");
      if(sender_number == PHONE){
        //Serial.println("doAction");
        doAction();
        //deleteSms();
      }
    }
  //////////////////////////////////////////////////
  }
  else{
  //The result of AT Command is "OK"
  }
}
//************************************************************
void extractSms(String buff){
   unsigned int index;
   Serial.println(buff);

    index =buff.indexOf(",");
    sms_status = buff.substring(1, index-1); 
    buff.remove(0, index+2);
    
    sender_number = buff.substring(0, 13);
    buff.remove(0,19);
   
    received_date = buff.substring(0, 20);
    buff.remove(0,buff.indexOf("\r"));
    buff.trim();
    
    index =buff.indexOf("\n\r");
    buff = buff.substring(0, index);
    buff.trim();
    msg = buff;
    buff = "";
    msg.toLowerCase();

    //Serial.println("----------------------------------");
    //Serial.println(sms_status);
    //Serial.println(sender_number);
    //Serial.println(received_date);
    //Serial.println(msg);
    //Serial.println("----------------------------------");
}
//***************************************************
void doAction(){
  //case sensitive
  if(msg == "bike on"){
    digitalWrite(ignition_switch, HIGH);
    Serial.println("Bike has ON");
    if(reply_status == true){
      sendSms("Bike has ON");
    }
  }
  else if(msg == "bike off"){
    digitalWrite(ignition_switch, LOW);
    Serial.println("Bike has OFF");
    if(reply_status == true){
      sendSms("Bike has OFF");
    }
  }
  else if(msg == "get location"){
    sendSmsGPS("Location");
  }
  else if(msg == "anti theft on"){
    anti_theft = true;
    if(reply_status == true){
      sendSms("Someone Trying to start your bike");
      sendSms("Bike Has OFF");
    }
  }
  else if(msg == "anti theft off"){
    anti_theft = false;
    if(reply_status == true){
      sendSms("Anti-Theft has OFF");
    }

  else if(msg == "reply on"){
    reply_status = true;
    sendSms("Reply has ON");
    }

  else if(msg == "reply off"){
    reply_status = false;
    }
  }
  else if(msg == "tracking on"){
    tracking_status = true;
    if(reply_status == true){
      sendSms("Live Tracking has ON");
    }
  }
  //yet to be implemented
  else if(msg == "tracking off"){
    tracking_status = false;
    if(reply_status == true){
      sendSms("Live Tracking has OFF");
    }
  }  
  else if(msg == "tracking status"){
    if (tracking_status == false){
      sendSms("Live Tracking has OFF");
    }
    else {
      sendSms("Live Tracking has ON");
    }
  }
  sms_status = "";
  sender_number="";
  received_date="";
  msg="";  
}
//***************************************************
void deleteSms()
{
  sendATcommand("AT+CMGD=1,4", "OK", 2000);
  Serial.println("All SMS are deleted.");
}
//***************************************************
void sendSmsGPS(String text)
{
  // Can take up to 60 seconds
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 2000;)
  {
    while (neogps.available())
    {
      if (gps.encode(neogps.read()))
      {
        newData = true;
      }
    }
  }
  if (newData)      //If newData is true
  {
    float flat, flon;
    unsigned long age;
    Serial.print("Latitude= "); 
    Serial.print(gps.location.lat(), 6);
    Serial.print(" Longitude= "); 
    Serial.println(gps.location.lng(), 6);
    newData = false;
    delay(300);
    ///*
    sim800.print("AT+CMGF=1\r");
    delay(1000);
    sim800.print("AT+CMGS=\""+PHONE+"\"\r");
    delay(1000);
    sim800.print("http://maps.google.com/maps?q=loc:");
    sim800.print(gps.location.lat(), 6);
    sim800.print(",");
    sim800.print(gps.location.lng(), 6);
    delay(100);
    sim800.write(0x1A); //ascii code for ctrl-26 //sim800.println((char)26); //ascii code for ctrl-26
    delay(1000);
    //Serial.println("GPS Message Sent Successfully.");
    //*/
  }
}
//***************************************************
void sendSms(String text)
{
    sim800.print("AT+CMGF=1\r");
    delay(1000);
    sim800.print("AT+CMGS=\""+PHONE+"\"\r");
    delay(1000);
    sim800.print(text);
    delay(100);
    sim800.write(0x1A); //ascii code for ctrl-26 //sim800.println((char)26); //ascii code for ctrl-26
    delay(1000);
    Serial.println("SMS Sent Successfully.");
}
//***************************************************
int8_t sendATcommand(char* ATcommand, char* expected_answer, unsigned int timeout){

    uint8_t x=0,  answer=0;
    char response[100];
    unsigned long previous;

    memset(response, '\0', 100);    // Initialice the string
    
    delay(100);
    
    while( sim800.available() > 0) sim800.read();    // Clean the input buffer
    
    if (ATcommand[0] != '\0')
    {
        sim800.println(ATcommand);    // Send the AT command 
    }


    x = 0;
    previous = millis();

    // this loop waits for the answer
    do{
        if(sim800.available() != 0){    // if there are data in the UART input buffer, reads it and checks for the asnwer
            response[x] = sim800.read();
            //Serial.print(response[x]);
            x++;
            if (strstr(response, expected_answer) != NULL)    // check if the desired answer (OK) is in the response of the module
            {
                answer = 1;
            }
        }
    }while((answer == 0) && ((millis() - previous) < timeout));    // Waits for the asnwer with time out

    return answer;
}
//***************************************************
boolean getIgnitionStatus()
{
  float val = 0;
  for(int i=1;i<=10;i++)
  { val = val + analogRead(ignition_sensor); }
  val = val/10;
  //Serial.println(val);
  if(val>90)
  {
    return true;
  }
  else if (val<50)
  {
    return false;
  }
}
//***************************************************
void setIgnition()
{
  ignition_status = getIgnitionStatus;
  if(ignition_status == false){
    //sim800l sleep mode turned off
    //must send sim800.print("AT"); here
    sim800.print("AT");
    sendATcommand("AT+CSCLK=0", "OK", 1000);
  }
  else if(ignition_status == true){
    //sim800l sleep mode turned on
    sendATcommand("AT+CSCLK=2", "OK", 1000);
  }
}
//***************************************************
int sendGpsToServer()
{
  //yet to be implemented  
}
