#include <Arduino.h>
//#include <Filters.h> //Easy library to do the calculations
#include <SPI.h>
#include <Ethernet.h>
#include <avr/wdt.h>

//#define MQTT_MAX_PACKET_SIZE 256

#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient

#include "timer-api.h"



//#define NDEBUG

//16 - постановка на охрану
//4 - снятие с охраны
#define CHECK_PULSES_INTERVAL 4
#define ALARM_PIN 2
#define ACMAINS_PIN 3
#define ETHERNET_RESET_PIN  4
#define MQTTUPDATEINTERVAL 121



uint8_t mqttUnaviable = 0;

byte mac[] = {
  0x00, 0xAA, 0xBC, 0xCC, 0xD2, 0x07
};
byte ip[] = { 10, 11, 13, 11 };
byte gateway[] = { 10, 11, 13, 1 };
byte subnet[] = { 255, 255, 255, 0 };


String MQTTserver = "192.168.100.110";
String MQTTuser = "openhabian";
String MQTTpwd = "damp19ax";
String MQTTtopic = "home/common/secalarm";
String MQTTClientID = "secalarmsystem";


bool bReportAll = false;

unsigned long lastMainsCheckUpdate = 0;

/*
AlarmState

0 - снята с охраны
1 - стоит на охране
2 - снятие с охраны
3 - постановка на охрану

*/
uint8_t currentAlarmState = 0;
uint8_t prevAlarmState = 5;

  uint8_t portState = 2;
  uint8_t prevPortState = 0;

//int co = 0;
int volatile newco = -1;
uint8_t interruptCounter = 0;
uint8_t interruptCounter2 = 0;

long Day=0;
int Hour =0;
int Minute=0;
int Second=0;
int SecondStamp=0;
int Once=0;



bool bDHCPError = false;


/*********** Mains detection variables *******************/


boolean current_Mains_state = false;
boolean prev_Mains_state = false;

/*********************************************************/




void uptime();


#include "services.h"

void pop ()
{
  newco++;
}


void timer_handle_interrupts(int timer) {


    interruptCounter++;
    interruptCounter2++;


    if( interruptCounter2 == MQTTUPDATEINTERVAL )
    {

              bReportAll = true;
              sendDataToMQTT();


       interruptCounter2 = 0;
    }




    if ( interruptCounter == CHECK_PULSES_INTERVAL )
    {
      interruptCounter = 0;
        #ifdef NDEBUG
          Serial.print("Frequency: ");
          Serial.println(newco, DEC);
        #endif


        if ( newco >= 3 &&  newco <= 5 )
        {
          //prevAlarmState = currentAlarmState;
          currentAlarmState = 2;
          #ifdef NDEBUG
          Serial.println("Срабатывание сигнализации.");
          #endif
        }
        else if ( newco >= 15 && newco <= 17 )
        {
          //prevAlarmState = currentAlarmState;
          currentAlarmState = 3;
          #ifdef NDEBUG
          Serial.println("Постановка на охрану.");
          #endif
        }
        else if ( newco <= 0 )
        {
          prevPortState = portState;
          portState = digitalRead(ALARM_PIN);


            if ( portState != prevPortState  ) //currentAlarmState != prevAlarmState ||
              {

                  #ifdef NDEBUG
                  Serial.print("Portstate: ");
                  Serial.println(portState, DEC);
                  #endif

                  switch ( portState )
                  {

                    case LOW:
                    {
                      //prevAlarmState = currentAlarmState;
                      currentAlarmState = 1;
                      #ifdef NDEBUG
                      Serial.println("Поставлено на охрану.");
                      #endif
                      break;
                    }
                    case HIGH:
                    {
                      //prevAlarmState = currentAlarmState;
                      currentAlarmState = 0;
                      #ifdef NDEBUG
                      Serial.println("Снято с охраны.");
                      #endif
                      break;
                    }
                  }
                }
              }


      newco = 0;


      if ( digitalRead( ACMAINS_PIN ) )
      {
        current_Mains_state = false;
      }
      else
      {
          current_Mains_state = true;  
      }
      
                  #ifdef NDEBUG
                  Serial.print("Power presence: ");
                  Serial.println(current_Mains_state);
                  #endif

      //report to MQTT
      if ( currentAlarmState != prevAlarmState || current_Mains_state != prev_Mains_state )
      {


          #ifdef NDEBUG
          Serial.println("Report to MQTT");
          #endif

          sendDataToMQTT();

        prevAlarmState = currentAlarmState;
        prev_Mains_state = current_Mains_state;
      }

    }

  }



void setup ()
{


    #ifdef NDEBUG
        Serial.begin(9600);
    //    Serial.println(co,DEC);
    #endif

    //pinMode(A1, INPUT_PULLUP);

    //set MQTT connect state
    digitalWrite(6, LOW);

    
    for (uint8_t i=0; i<=4; i++)
    {

      for (int y=0; y<=255; y++)
      {
        analogWrite(5, y);
        delay (5);
      }
      delay (5);

      for (int z=255; z>=0; z--)
      {
        analogWrite(5, z);
        delay (5);
      }
      delay (5);
    }


  //digitalWrite(5, LOW);


// Reset the W5500 module
  pinMode(ETHERNET_RESET_PIN, OUTPUT);
  
  
  
 

  
  //digitalWrite(ETHERNET_RESET_PIN, LOW);
  //delay(800); //100
  //digitalWrite(ETHERNET_RESET_PIN, HIGH);
  //delay(150); //100



  digitalWrite(ETHERNET_RESET_PIN, LOW);
  delay(800); //100
  digitalWrite(ETHERNET_RESET_PIN, HIGH);
  delay(800); //100



if ( Ethernet.begin(mac) == 0)
{

  //Enable watchdog timer
  // wdt_enable(WDTO_8S);


#ifdef NDEBUG

    Serial.println("Resetting");
#endif

  resetBoard();


}

#ifdef NDEBUG

    Serial.println(DisplayAddress(Ethernet.localIP()));
#endif

  pinMode(ALARM_PIN, INPUT_PULLUP);
  pinMode(ACMAINS_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ALARM_PIN),pop,FALLING);

timer_init_ISR_1Hz(TIMER_DEFAULT);



}


void loop ()
{
  switch (Ethernet.maintain()) {
      case 1:
        //renewed fail

             resetBoard();
        break;

      case 2:
        //renewed success

        break;

      case 3:
        //rebind fail

             resetBoard();
        break;

      case 4:
        //rebind success

        break;

      default:
        //nothing happened
        break;
    }


    uptime();



}


//************************ Uptime Code - Makes a count of the total up time since last start ****************//
//It will work for any main loop's, that loop moret han twice a second: not good for long delays etc
void uptime(){
//** Checks For a Second Change *****//
if(millis()%1000<=500&&Once==0){
SecondStamp=1;
Once=1;
}
//** Makes Sure Second Count doesnt happen more than once a Second **//
if(millis()%1000>500){
Once=0;
}




                         if(SecondStamp==1){
                           Second++;
                           SecondStamp=0;
                           //print_Uptime();

                         if (Second==60){
                          Minute++;
                          Second=0;
                          if (Minute==60){
                          Minute=0;
                          Hour++;

                         if (Hour==24){
                          Hour=0;
                          Day++;
                         }
                         }
                         }
                         }


}
