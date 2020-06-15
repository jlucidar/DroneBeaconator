// Author : Jimmy Lucidarme

// specs available here : https://www.legifrance.gouv.fr/affichTexte.do;jsessionid=4D49B90D1AF4E7C145B415A7C773010A.tplgfr30s_1?cidTexte=JORFTEXT000039685188&dateTexte=20191229&categorieLien=cid&fbclid=IwAR3oJEhrYCZRkfthYyrYa0862_dC8tYBXXMo2LvbhiRuter2xIJzInK73hw#JORFTEXT000039685188

#include <ESP8266WiFi.h>
#include <MSP.h>

extern "C" {
  #include "user_interface.h"
}

MSP msp;

byte channel;
byte i;
byte count;

byte protocol_version = 0x01; // Version du protocole
char* fr_id = "LCDESS000000000000LCDESS000001"; // Identifiant FR sur 30 caractères ( LCD = Trigramme constructeur / ESS = id system / SN )

//char* uas_psn = "0000400000000000000000000";
//Identifiant ANSI CTA 2063 UAS (numéro de série physique - PSN)  [4 Character MFR CODE] [1 character LENGTH CODE][20 character SERIAL NUMBER]

double lat = 179.12345; //Latitude courante aéronef (signée)  
double lon = 48.15278; //Longitude courante aéronef (signée)  
int16_t alt = 102; //Altitude courante aéronef (signée)  
int16_t height = 0; //Hauteur courante aéronef (signée)  
double home_lat = -179.12345; //Latitude courante aéronef (signée)  
double home_lon = -48.15278; //Longitude courante aéronef (signée)  
int8_t horizontal_speed = 10; //Vitesse horizontale 
int16_t true_heading = -259;// Route vraie


byte wifipkt[128] = {   0x80, 0x00, //Frame Control 
                        0x00, 0x00, //Duration
                /*4*/   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, //Destination address 
                /*10*/  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, //Source address - overwritten later
                /*16*/  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, //BSSID - overwritten to the same as the source address
                /*22*/  0xc0, 0x6c, //Seq-ctl
                //Frame body starts here
                /*24*/  0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, //timestamp - the number of microseconds the AP has been active
                /*32*/  0xFF, 0x00, //Beacon interval
                /*34*/  0x01, 0x04, //Capability info
                /*36*/  0x00, 0x00, //SSID
                /*38*/  0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c, //supported rate
                /*48*/  0x03, 0x01, 0x06 /*DSSS (Current Channel)*/ 
                };

byte VSblock[] = {      0xDD, //VS block id
                /*1*/   0x04, //length (overrided later )
                /*2*/   0x6A, 0x5C, 0x35, //OUI
                /*5*/   0x01 // VS Type
                };


int8_t payloadSize;

void printHex(byte num) {
  char hexCar[2];

  sprintf(hexCar, "%02X", num);
  Serial.print(hexCar);
}

byte* getVSPayload(){
    // version segment
    byte protocol_version_tlv[3] = {0x01,0x01};
    protocol_version_tlv[2] = protocol_version;

    // id segment
    byte id_tlv[32] = {0x02,0x1E};
    for (i=0; i<30; i++) {
       id_tlv[2+i]=fr_id[i];
    }

    //position
    
    //lat segment
    byte lat_tlv[6] = {0x04,0x04};
    int32_t encoded_lat = 0;
    encoded_lat = (int32_t) (100000 * lat);
    lat_tlv[2] = encoded_lat >> 24;
    lat_tlv[3] = encoded_lat >> 16;
    lat_tlv[4] = encoded_lat >> 8;
    lat_tlv[5] = encoded_lat;
    
    //lon segment
    byte lon_tlv[6] = {0x05,0x04};
    int32_t encoded_lon = 0;
    encoded_lon = (int32_t) (100000 * lon);
    lon_tlv[2] = encoded_lon >> 24;
    lon_tlv[3] = encoded_lon >> 16;
    lon_tlv[4] = encoded_lon >> 8;
    lon_tlv[5] = encoded_lon;

    //alt segment
    byte alt_tlv[4] = {0x06,0x02};
    alt_tlv[2] = alt >> 8;
    alt_tlv[3] = alt;

    //home_lat segment
    byte home_lat_tlv[6] = {0x08,0x04};
    int32_t encoded_home_lat = 0;
    encoded_home_lat = (int32_t) (100000 * home_lat);
    home_lat_tlv[2] = encoded_home_lat >> 24;
    home_lat_tlv[3] = encoded_home_lat >> 16;
    home_lat_tlv[4] = encoded_home_lat >> 8;
    home_lat_tlv[5] = encoded_home_lat;
    
    //home_lon segment
    byte home_lon_tlv[6] = {0x09,0x04};
    int32_t encoded_home_lon = 0;
    encoded_home_lon = (int32_t) (100000 * home_lon);
    home_lon_tlv[2] = encoded_home_lon >> 24;
    home_lon_tlv[3] = encoded_home_lon >> 16;
    home_lon_tlv[4] = encoded_home_lon >> 8;
    home_lon_tlv[5] = encoded_home_lon;

    //horizontal_speed segment
    byte horizontal_speed_tlv[3] = {0x0A,0x01};
    horizontal_speed_tlv[2] = horizontal_speed;

    //true_heading segment
    byte true_heading_tlv[4] = {0x0B,0x02};
    true_heading_tlv[2] = true_heading >> 8;
    true_heading_tlv[3] = true_heading;

    
    payloadSize = 3 + 32 + 6 + 6 + 4 + 6 + 6 + 3 + 4 ;
    byte payload[payloadSize];

    int j;
    int payloadCounter = 0;
    
    for (j=0; j<sizeof(protocol_version_tlv); j++) {
       payload[payloadCounter++]=protocol_version_tlv[j];
    }
    for (j=0; j<sizeof(id_tlv); j++) {
       payload[payloadCounter++]=id_tlv[j];
    }
    for (j=0; j<sizeof(lat_tlv); j++) {
       payload[payloadCounter++]=lat_tlv[j];
    }
    for (j=0; j<sizeof(lon_tlv); j++) {
       payload[payloadCounter++]=lon_tlv[j];
    }
    for (j=0; j<sizeof(alt_tlv); j++) {
       payload[payloadCounter++]=alt_tlv[j];
    }
    for (j=0; j<sizeof(home_lat_tlv); j++) {
       payload[payloadCounter++]=home_lat_tlv[j];
    }
    for (j=0; j<sizeof(home_lon_tlv); j++) {
       payload[payloadCounter++]=home_lon_tlv[j];
    }
    for (j=0; j<sizeof(horizontal_speed_tlv); j++) {
       payload[payloadCounter++]=horizontal_speed_tlv[j];
    }
    for (j=0; j<sizeof(true_heading_tlv); j++) {
       payload[payloadCounter++]=true_heading_tlv[j];
    }
    
    return payload;
}

void setup() {
  delay(500);
  wifi_set_opmode(STATION_MODE);
  wifi_promiscuous_enable(1); 
  Serial.begin(115200);
  msp.begin(Serial);
}

void loop() {

    //randomize mac address
    wifipkt[10] = wifipkt[16] = random(256);
    wifipkt[11] = wifipkt[17] = random(256);
    wifipkt[12] = wifipkt[18] = random(256);
    wifipkt[13] = wifipkt[19] = random(256);
    wifipkt[14] = wifipkt[20] = random(256);
    wifipkt[15] = wifipkt[21] = random(256);

    //wifi pkt length 
    count=51;

    // set channel
    channel = 6; 
    wifi_set_channel(channel);
    wifipkt[count-1] = channel;

    //get GPS info from FC
    Serial.println("sending msp request !");
    msp_raw_gps_t gps;
    if (msp.request(MSP_RAW_GPS, &gps, sizeof(gps))) {
      uint8_t  fixType = gps.fixType;       // MSP_GPS_NO_FIX, MSP_GPS_FIX_2D, MSP_GPS_FIX_3D
      uint8_t  numSat = gps.numSat;
      lat = gps.lat / 10000000;           // 1 / 10000000 deg
      lon = gps.lon / 10000000;           // 1 / 10000000 deg
      alt = gps.alt;           // meters
      horizontal_speed = gps.groundSpeed * 10;   // cm/s
      true_heading = gps.groundCourse / 10;  // unit: degree x 10
    }else{
      Serial.println();
      Serial.println("no answer :(");
    }
    // VS block header
    for (i=0; i<sizeof(VSblock); i++) {
       wifipkt[count++]=VSblock[i];
    }
    
    byte* VSpayload;
    VSpayload = getVSPayload();
    
    wifipkt[count-5]= payloadSize + 4; // set payload size (payload + Oui + vs type)
    
    for (i=0; i<payloadSize; i++) {
       wifipkt[count++]=VSpayload[i];
    }

    /* frame debugging *//*
    for(i=0; i<count; i++){
        printHex(wifipkt[i]);
    }
    Serial.println();
    *//* end of frame debugging */
    
    //send beacon
    wifi_send_pkt_freedom(wifipkt, count, 0);
    wifi_send_pkt_freedom(wifipkt, count, 0);
    wifi_send_pkt_freedom(wifipkt, count, 0);
    delay(500);
}
