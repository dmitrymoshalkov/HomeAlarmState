#ifndef SERVICES_H
#define SERVICES_H


#include <PubSubClient.h>



EthernetClient ethClient;
PubSubClient mqttAPI(ethClient);




void checkMQTT();

/*
String httpCodeStr(int code) {
  switch(code) {
    case -1:  return "CONNECTION REFUSED";
    case -2:  return "SEND HEADER FAILED";
    case -3:  return "SEND PAYLOAD FAILED";
    case -4:  return "NOT CONNECTED";
    case -5:  return "CONNECTION LOST";
    case -6:  return "NO STREAM";
    case -7:  return "NO HTTP SERVER";
    case -8:  return "TOO LESS RAM";
    case -9:  return "ENCODING";
    case -10: return "STREAM WRITE";
    case -11: return "READ TIMEOUT";
     default: return  http.codeTranslate(code);
  }
}
*/


String DisplayAddress(IPAddress address)
{
 return String(address[0]) + "." +
        String(address[1]) + "." +
        String(address[2]) + "." +
        String(address[3]);
}


String mqttCodeStr(int code) {
  switch (code) {
    case -4: return "CONNECTION TIMEOUT";
    case -3: return "CONNECTION LOST";
    case -2: return "CONNECT FAILED";
    case -1: return "MQTT DISCONNECTED";
    case  0: return "CONNECTED";
    case  1: return "CONNECT BAD PROTOCOL";
    case  2: return "CONNECT BAD CLIENT ID";
    case  3: return "CONNECT UNAVAILABLE";
    case  4: return "CONNECT BAD CREDENTIALS";
    case  5: return "CONNECT UNAUTHORIZED";
    default: return String(code);
  }
}


void sendDataToMQTT();


void resetBoard()
{
  //while (true) {
  //  delay(1);
  //}

  for (uint8_t i=0; i<=5; i++)
  {
       analogWrite(5, 255);
       delay(300);
       analogWrite(5, 0);     
  }

  digitalWrite(ETHERNET_RESET_PIN, LOW);
  delay(800); //100
  digitalWrite(ETHERNET_RESET_PIN, HIGH);
  
asm volatile("jmp 0x00");

}




bool mqttPublish(String topic, String data) {
yield();
  if (MQTTtopic.length()) topic = MQTTtopic + "/" + topic;
  return mqttAPI.publish(topic.c_str(), data.c_str(), true);
}
bool mqttPublish(String topic, float data) { return mqttPublish(topic, String(data)); }
bool mqttPublish(String topic, int32_t data) { return mqttPublish(topic, String(data)); }
bool mqttPublish(String topic, uint32_t data) { return mqttPublish(topic, String(data)); }


void sendDataToMQTT() {
  if (Ethernet.localIP() and MQTTserver.length() ) {

    mqttAPI.setServer(MQTTserver.c_str(), 1883);
    mqttAPI.connect(MQTTClientID.c_str(), MQTTuser.c_str(), MQTTpwd.c_str());

    if (mqttAPI.connected()) {

         analogWrite(6, 255);

      #ifdef NDEBUG
        Serial.println(F("send to MQTT"));
        #endif

        String state = "DISARMED";
        String state1;
        String powerstate;


        /*
        AlarmState

        0 - снята с охраны
        1 - стоит на охране
        2 - снятие с охраны
        3 - постановка на охрану

        */

        switch (currentAlarmState)
        {
          case 0:
            state = "DISARMED";
            break;
            case 1:
            state = "ARMED";
            break;
            case 2:
            state = "DISARMING";
            break;
            case 3:
            state = "ARMING";
            break;
          }


          mqttPublish("state/status",   state );

          if ( currentAlarmState == 0 || currentAlarmState == 1 )
          {
            switch (currentAlarmState)
            {
              case 0:
              state1 = "OFF";
              break;
              case 1:
              state1 = "ON";
              break;
            }

            mqttPublish("state",   state1 );
          }


                    if ( current_Mains_state )
                    {
                        powerstate = "ON";
                    }
                    else
                    {
                        powerstate = "OFF"; 
                    }

            mqttPublish("state/power",   powerstate );


          if (bReportAll)
          {
               


            mqttPublish("state/ip",   DisplayAddress(Ethernet.localIP()) );

            String sUptime = String(Day) + "d " + String(Hour) + ":" + String(Minute) + ":" + String(Second);
            mqttPublish("state/uptime",   sUptime );



            bReportAll = false;
          }

          #ifdef NDEBUG
          Serial.println( "answer: " +  mqttCodeStr(mqttAPI.state()));
          #endif

                   analogWrite(6, 0);

      }
  }

}






#endif
