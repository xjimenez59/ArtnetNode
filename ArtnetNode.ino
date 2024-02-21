
/*
  v7 du script : introduit un buffer de "n" frames, pour compenser les soubresauts de transmission UDP. 
  Pas d'utilisation des interruptions pour cadencer l'affichage des frames ..
*/

#define SCRIPT_VERSION "script version 7"
//#define DEBUG


#include "Arduino.h"
#include "WiFi.h"
#include <ESP32Ping.h>
#include "customConfigAssist.h"
#include "customAssistHelper.h"  // Config assist helper class

#include "artnetESP32V2.h"

//#include "customI2SClockLessLedDriverEsp32s3.h"
#define FASTLED_INTERNAL
#include "FastLED.h"

#include "home_template.h"


//----- frame buffer stuff
int frame_buffer_size = 20;
int frame_buffer_low_count = 4;
uint8_t *frameBuffer;
int lastReceivedIndex = -1;    //-- emplacement de la prochaine frame reçue
int nextToShowIndex = -1;  //-- emplacement de la prochaine frame à afficher
int shownFrames = 0;
int totalLost = 0;
int totalEmptyBuffer = 0;


//-- leds stuff
const byte dataPin = 5;  //-- 5 sur l'esp32c3 GPIO5 = A3 = D3
// int pins[16]={2,4,5,12,13,14,15,17,16,19,21,22,23,25,26};
int pins[1]={dataPin};
int numLeds = 50;
uint8_t *leds;
//I2SClocklessLedDriveresp32S3 ledCtrl;
CLEDController* ledCtrl;
TaskHandle_t showLedsTaskHandle = NULL;
int nextDelay = 300;
unsigned long lastResume = 0;



//-- ConfigAssist stuff
#include "config_assist.h"
WebServer server(80);
ConfigAssist conf("/config.ini",VARIABLES_DEF_JSON);
bool isConfigReady = false;


//-- Artnet stuff
artnetESP32V2 artnet=artnetESP32V2();
char strbuf[200];
int frameSize;


// ----------------
// Reception des "frames"
// On les accumule dans un buffer ; si l'affichage a trop tardé, on écrase la prochaine frame à afficher.
void incBufIndex(int *index) {
  (*index)++;
  if (*index == frame_buffer_size) {
    *index = 0;
    }
}


void pushInboundFrame(subArtnet *subartnet){

  if (lastReceivedIndex == -1) {
    lastReceivedIndex = 0;
  } else {
    incBufIndex(&lastReceivedIndex);
  }

  memmove( &frameBuffer[lastReceivedIndex * frameSize], subartnet->data, subartnet->len );
  bool lost = false;
  if (lastReceivedIndex == nextToShowIndex) {
    incBufIndex(&nextToShowIndex); //-- on n'avait pas encore affiché cette frame .. tant pis, trop tard.
    lost = true;
  }
  if (nextToShowIndex == -1) {
    nextToShowIndex = lastReceivedIndex;  // le buffer n'est plus vide
  }
  #ifdef DEBUG
  Serial.print("+");
  if (lost) {
    totalLost++;
    sprintf(strbuf,"buflen: %u [%u,%u] shown: %d lost: %d (%2d.1%%) empty buf: %d",bufferLen(),nextToShowIndex,lastReceivedIndex,shownFrames,totalLost,(shownFrames == 0) ? 0.0 : totalLost*100/shownFrames,totalEmptyBuffer);
    Serial.println(strbuf);

  }
  #endif
}


int bufferLen() {
  volatile int len;
  if (nextToShowIndex == -1) {
    len = 0;
  } else {
    len = lastReceivedIndex - nextToShowIndex;
    if (len <0) len += frame_buffer_size;
    len++;
  }
  return len; 
}

// -------------
// Affichage de la prochaine frame
//
void showNextFrame(void *arg) {
  while (1) {
    
    vTaskSuspend(NULL);
      if (nextToShowIndex != -1) {
        //memmove(leds, &frameBuffer[nextToShowIndex * frameSize], frameSize);
        //ledCtrl.show();

        ledCtrl->setLeds((CRGB *) &frameBuffer[nextToShowIndex * frameSize],numLeds);
        FastLED.show();
        
        #ifdef DEBUG
          //sprintf(strbuf,"show frame %u",nextToShowIndex);
          //Serial.println(strbuf);
          Serial.print("-");
        #endif
      
        if (nextToShowIndex == lastReceivedIndex) {
          nextToShowIndex = -1;   //on vient d'afficher le dernier element du buffer.
          digitalWrite(LED_BUILTIN, LOW);  //-- buffer vide : on allume la led
          totalEmptyBuffer++;
        } else {
          incBufIndex(&nextToShowIndex);
          digitalWrite(LED_BUILTIN, HIGH);

        }
        shownFrames++;
      } 
  }
}


void setup() {
  Serial.begin(2000000);
  Serial.println(SCRIPT_VERSION);     
  pinMode(LED_BUILTIN,OUTPUT);
  
  initConfig();

  if (isConfigReady) {
    delay(1000);
    numLeds = conf["leds_nb"].toInt();
    frameSize = numLeds * sizeof(uint8_t) * 3;  // leds RGB donc 3  octets par led
    initLeds();
    initArtnet();
    ledOff();
  }

  
  
  
  sprintf(strbuf, "Chip model: %s, %d MHz, %d coeurs",ESP.getChipModel(),ESP.getCpuFreqMHz(),ESP.getChipCores());
  Serial.println(strbuf);
  sprintf(strbuf, "Free heap: %d / %d %d max block",ESP.getFreeHeap(),ESP.getHeapSize(),ESP.getMaxAllocHeap());
  Serial.println(strbuf);
  sprintf(strbuf,"Free PSRAM: %d / %d %d max block",ESP.getFreePsram(),ESP.getPsramSize(),ESP.getMaxAllocPsram());
  Serial.println(strbuf);
}

int nextHandleClient= millis() + 1000;
void loop() {
  unsigned long now = millis();
  if(now - lastResume >= nextDelay) {
  
    if (isConfigReady) {
      lastResume = now;
      nextDelay = 50; //-- par défaut, 50ms (20 frames/s)
      if (bufferLen() < frame_buffer_low_count) nextDelay = 100; // en cas de retard, on ralentit le rythme d'affichege 
      vTaskResume(showLedsTaskHandle);
    } else {
      blink(10,200);
    }
    if(now > nextHandleClient) {
      server.handleClient();
      nextHandleClient = now + 250;
    }
  }
  
  yield(); 
  //vTaskDelete(NULL);
}

void handleRoot() {
  sendHomePage(&server,WiFi.getHostname(),shownFrames,totalLost,totalEmptyBuffer);
}

void initConfig() {

  if(!STORAGE.begin(true)) Serial.println("ESP32 storage init failed!");
  //conf.deleteConfig(); // Uncomment to remove ini file and re-built

  ConfigAssistHelper confHelper(conf);
  bool bConn = confHelper.connectToNetwork();
  conf.setup(server, !bConn); 
  if(bConn) {

    //-- cxn ok, on attend de pouvoir "pinguer" la gateway ..
    IPAddress gwip = WiFi.gatewayIP();
    int ping_count = 5;
    while (Ping.ping(gwip) == false && ping_count-- > 0) {
      Serial.println("echec ping");
      blink(3,100);
      delay(250);
    };

    isConfigReady = true;
    server.on("/", handleRoot);
  }
  server.begin();
  
}

void initLeds() {
  leds = (uint8_t *) malloc(frameSize);
  //ledCtrl.initled(leds,pins,1,numLeds,(enum colorarrangment) conf["leds_color_order"].toInt());   // un seul "strip" , donc toutes les leds sur ce strip.
  ledCtrl = &FastLED.addLeds<WS2812, dataPin, RGB>((CRGB *) leds, numLeds);
  //ledCtrl.setBrightness(20);
  //--- create display task
  lastResume = millis();
  nextDelay = conf["start_delay"].toInt();
  xTaskCreatePinnedToCore(showNextFrame, "Show next frame task", 4096, NULL, 10, &showLedsTaskHandle,0);
  //xTaskCreate(showNextFrame, "Show next frame task", 4096, NULL, 10, &showLedsTaskHandle);
}

void initArtnet() {
  /* We add the subartnet reciever. Here we only need one
  * addSubartnet (Start universe,size of data,number of channel per universe,callback function)
  * NUM_LEDS_PER_STRIP * NUMSTRIPS is the total number of leds 
  * NUM_LEDS_PER_STRIP * NUMSTRIPS * NB_CHANNEL_PER_LED is the total number of channels needed here 3 channel per led (R,G,B)
  */ 
  frame_buffer_size = conf["frame_buf_nb"].toInt();
  frame_buffer_low_count = conf["frame_buf_low"].toInt();

  frameBuffer = (uint8_t *) malloc(frame_buffer_size * frameSize);

  subArtnet* sub =  artnet.addSubArtnet(conf["start_universe"].toInt() ,frameSize, 170 * sizeof(uint8_t) * 3 ,&pushInboundFrame);
    
  if(artnet.listen(6454)) {
      Serial.print("artnet Listening on IP: ");
      Serial.println(WiFi.localIP());
  }

}

void ledOn() {
  digitalWrite(LED_BUILTIN,LOW);
}
void ledOff(){
  digitalWrite(LED_BUILTIN,HIGH);
}
void blink(int nb, int ms) {
  //int old_level = digitalRead(LED_BUILTIN);
  for (int i=0; i<nb; i++){
    digitalWrite(LED_BUILTIN,LOW);
    vTaskDelay(ms/2);
    digitalWrite(LED_BUILTIN,HIGH);
    vTaskDelay(ms/2);
  }
  digitalWrite(LED_BUILTIN,HIGH);
}


