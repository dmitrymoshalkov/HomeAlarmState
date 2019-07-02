#include <Arduino.h>
//#include <SPI.h>
#include <Ethernet.h>
//#include <SD.h>
//#include <EthernetUdp.h>
#include <avr/wdt.h>

//#define MQTT_MAX_PACKET_SIZE 256

#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient

#include "timer-api.h"



#define NDEBUG

//16 - постановка на охрану
//4 - снятие с охраны
#define CHECK_PULSES_INTERVAL 4
#define ALARM_PIN 3
#define ETHERNET_RESET_PIN  4
#define MQTTUPDATEINTERVAL 120000 //3760000 //940000
#define MQTTCHECKINTERVAL 16 //840000 //940000

uint8_t mqttUnaviable = 0;

byte mac[] = {
  0x00, 0xAA, 0xBB, 0xCC, 0xD1, 0x07
};
byte ip[] = { 10, 11, 13, 32 };
byte gateway[] = { 10, 11, 13, 1 };
byte subnet[] = { 255, 255, 255, 0 };


String MQTTserver = "192.168.100.124";
String MQTTuser = "openhabian";
String MQTTpwd = "damp19ax";
String MQTTtopic = "home/common/secalarm";
String MQTTClientID = "secalarmsystem";


bool bReportAll = false;

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

int co = 0;
int volatile newco = -1;
uint8_t interruptCounter = 0;
uint8_t interruptCounter2 = 0;

long Day=0;
int Hour =0;
int Minute=0;
int Second=0;
int SecondStamp=0;
int Once=0;


long lastMQTTUpdate = 0;
long lastMQTTCheckUpdate = 0;

bool bDHCPError = false;


void uptime();


#include "services.h"

void pop ()
{
  newco++;
}


void timer_handle_interrupts(int timer) {


    interruptCounter++;
    interruptCounter2++;


    if( interruptCounter2 == MQTTCHECKINTERVAL )
    {

      #ifdef NDEBUG
        Serial.println("Check MQTT ");
      #endif

       checkMQTT();

       if (mqttAPI.connected())
       {
         digitalWrite(6, HIGH);
         mqttUnaviable =0;

         #ifdef NDEBUG
           Serial.println("MQTT API connected");
         #endif

       } else if (!bDHCPError)
       {
           //setColorState(3);
           digitalWrite(6, LOW);
           mqttUnaviable++;

           #ifdef NDEBUG
             Serial.print("mqttUnaviable: ");
             Serial.println(mqttUnaviable, DEC);
           #endif

           if (mqttUnaviable > 21)
           {
              resetBoard();
           }
       }

       interruptCounter2 = 0;
    }




    if ( interruptCounter == CHECK_PULSES_INTERVAL )
    {
      interruptCounter = 0;
        #ifdef NDEBUG
          Serial.print("Frequency: ");
          Serial.println(newco, DEC);
        #endif


        if ( newco >= 2 &&  newco <= 8 ) //&& currentAlarmState != prevAlarmState
        {
          //prevAlarmState = currentAlarmState;
          currentAlarmState = 2;
          #ifdef NDEBUG
          Serial.println("Срабатывание сигнализации.");
          #endif
        }
        else if ( newco >= 10 && newco <= 20 ) //&& currentAlarmState != prevAlarmState
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

      //report to MQTT
      if ( currentAlarmState != prevAlarmState  )
      {

        //if (prevAlarmState != 5)
        //{
          #ifdef NDEBUG
          Serial.println("Report to MQTT");
          #endif

          sendDataToMQTT();
        //}
        prevAlarmState = currentAlarmState;
      }

    }

  }



void setup ()
{
  pinMode(ALARM_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ALARM_PIN),pop,FALLING);

    #ifdef NDEBUG
        Serial.begin(9600);
        Serial.println(co,DEC);
    #endif

    //digitalWrite(5, HIGH);

    //set MQTT connect state
    digitalWrite(6, LOW);

    digitalWrite(5, HIGH);
    delay(20000);
    digitalWrite(5, LOW);

// Reset the W5500 module
  pinMode(ETHERNET_RESET_PIN, OUTPUT);
  digitalWrite(ETHERNET_RESET_PIN, HIGH);
  digitalWrite(ETHERNET_RESET_PIN, LOW);
  delay(800); //100
  digitalWrite(ETHERNET_RESET_PIN, HIGH);
  delay(800); //100

if ( Ethernet.begin(mac) == 0)
{

  //Enable watchdog timer
  // wdt_enable(WDTO_8S);

  resetBoard();


}

#ifdef NDEBUG

    Serial.println(DisplayAddress(Ethernet.localIP()));
#endif



timer_init_ISR_1Hz(TIMER_DEFAULT);

setupMQTT();

}


void loop ()
{
  switch (Ethernet.maintain()) {
      case 1:
        //renewed fail
        //Serial.println("Error: renewed fail");
             //setColorState(2);
             //delay(1000);
             resetBoard();
        break;

      case 2:
        //renewed success
        //Serial.println("Renewed success");
        //print your local IP address:
        //Serial.print("My IP address: ");
        //Serial.println(Ethernet.localIP());
        break;

      case 3:
        //rebind fail
        //Serial.println("Error: rebind fail");
             //setColorState(2);
             //delay(1000);
             resetBoard();
        break;

      case 4:
        //rebind success
        //Serial.println("Rebind success");
        //print your local IP address:
        //Serial.print("My IP address: ");
        //Serial.println(Ethernet.localIP());
        break;

      default:
        //nothing happened
        break;
    }





    // and set the new light colors
    //if (millis() > lastupdate + INTERVAL) {
    //  updateLights();
    //  lastupdate = millis();
    //}



    /* Обработчик MQTT для режима "подписчика" */
    mqttAPI.loop();

/*
    unsigned long currentCheckMQTTMillis = millis();

    if( (currentCheckMQTTMillis - lastMQTTCheckUpdate ) > MQTTCHECKINTERVAL )
    {

      #ifdef NDEBUG
        Serial.println("Check MQTT ");
      #endif

       checkMQTT();

       if (mqttAPI.connected())
       {
         digitalWrite(6, HIGH);
         mqttUnaviable =0;

         #ifdef NDEBUG
           Serial.println("MQTT API connected");
         #endif

       } else if (!bDHCPError)
       {
           //setColorState(3);
           digitalWrite(6, LOW);
           mqttUnaviable++;

           #ifdef NDEBUG
             Serial.print("mqttUnaviable: ");
             Serial.println(mqttUnaviable, DEC);
           #endif

           if (mqttUnaviable > 21)
           {
              resetBoard();
           }
       }

         lastMQTTCheckUpdate = currentCheckMQTTMillis;
    }
*/

    unsigned long currentMQTTMillis = millis();

       if( (currentMQTTMillis - lastMQTTUpdate ) > MQTTUPDATEINTERVAL )
       {
            bReportAll = true;
            sendDataToMQTT();


         lastMQTTUpdate = currentMQTTMillis;
       }

    //MQTTtimer.run();

    uptime();

    //reset watchdog timer
    //wdt_reset();

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
