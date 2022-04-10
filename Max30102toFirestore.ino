
/**
   Created by K. Suwatchai (Mobizt)

   Email: k_suwatchai@hotmail.com

   Github: https://github.com/mobizt/Firebase-ESP-Client

   Copyright (c) 2022 mobizt

*/

//This example shows how to add the value to array field. This operation required Email/password, custom or OAUth2.0 authentication.

#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif



//NTP
#include <NTPClient.h>  // need to install 
#include <WiFiUdp.h>
#include <TimeLib.h> // TimeLib need to install 


//MAX30102

#include <Wire.h>
#include "MAX30105.h"           //MAX3010x library
#include "heartRate.h"          //Heart rate calculating algorithm
//#include "ESP32Servo.h"
MAX30105 particleSensor;
int Tonepin = 4;
//計算心跳用變數
const byte RATE_SIZE = 10; //多少平均數量
byte rates[RATE_SIZE]; //心跳陣列
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred
float beatsPerMinute;
int beatAvg;


#define FINGER_ON 7000 //紅外線最小量（判斷手指有沒有上）
#define MINIMUM_SPO2 90.0//血氧最小量


//datacount
#define beep_pin 4
int datacount = 0;
#define datacount_max 333 //60*1000ms/180ms =333




//計算血氧用變數
double avered = 0;
double aveir = 0;
double sumirrms = 0;
double sumredrms = 0;

double SpO2 = 0;
double ESpO2 = 90.0;//初始值
double FSpO2 = 0.7; //filter factor for estimated SpO2
double frate = 0.95; //low pass filter for IR/red LED value to eliminate AC component
int i = 0;
int Num = 30;//取樣100次才計算1次


#include <Firebase_ESP_Client.h>

//Provide the token generation process info.
#include <addons/TokenHelper.h>

/* 1. Define the WiFi credentials */
#define WIFI_SSID "OpenWrt"
#define WIFI_PASSWORD "08041218"

//For the following credentials, see examples/Authentications/SignInAsUser/EmailPassword/EmailPassword.ino

/* 2. Define the API Key */
//#define API_KEY "AIzaSyBpGg5cMuJyWIsHMU-PTxw4T20v_eCDjjQ"
#define API_KEY "AIzaSyDI3SJgUyqu1qJiT5RxoiDX49ck8S_LyeM"


/* 3. Define the project ID */
#define FIREBASE_PROJECT_ID "heart-2c8da"

/* 4. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL "lienchiachun524@gmail.com"
#define USER_PASSWORD "lienchiachun8012"

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long dataMillis = 0;


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String formattedDate;
String dayStamp;
String timeStamp;

void setup()
{

  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  timeClient.begin();
  timeClient.setTimeOffset(28800);//GMT +8 = 28800
  delay(500);
  timeClient.forceUpdate();



  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the user sign in credentials */
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);

  Firebase.reconnectWiFi(true);



  max30102_begin();
}

void loop()
{
  max30102_run();
  if (Firebase.ready() && (datacount > datacount_max))
    //if (Firebase.ready() && (millis() - dataMillis > 60000 || dataMillis == 0))
  {
    datacount = 0;

      digitalWrite(beep_pin, HIGH);
      delay(150);                       // wait for a second
      digitalWrite(beep_pin, LOW);
      delay(150);
      digitalWrite(beep_pin, HIGH);
      delay(150);                       // wait for a second
      digitalWrite(beep_pin, LOW);
    dataMillis = millis();


    Serial.print("Commit a document (append array)... ");

    //The dyamic array of write object fb_esp_firestore_document_write_t.
    std::vector<struct fb_esp_firestore_document_write_t> writes;

    //A write object that will be written to the document.
    struct fb_esp_firestore_document_write_t transform_write;

    //Set the write object write operation type.
    //fb_esp_firestore_document_write_type_update,
    //fb_esp_firestore_document_write_type_delete,
    //fb_esp_firestore_document_write_type_transform
    transform_write.type = fb_esp_firestore_document_write_type_transform;


    timeClient.forceUpdate();
    Serial.println(timeClient.getEpochTime());
    unsigned long epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime ((time_t *)&epochTime);
    int monthDay = ptm->tm_mday;
    int currentMonth = ptm->tm_mon + 1;
    int currentYear = ptm->tm_year + 1900;


    String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay);
    Serial.print("Current date: ");
    Serial.println(currentDate);
    //Set the document path of document to write (transform)
    String doc_txt = "max30102/" + String(timeClient.getEpochTime());
    transform_write.document_transform.transform_document_path = doc_txt;

    //Set a transformation of a field of the document.
    struct fb_esp_firestore_document_write_field_transforms_t field_transforms;

    //Set field path to write.
    field_transforms.fieldPath = "data";

    //Set the transformation type.
    //fb_esp_firestore_transform_type_set_to_server_value,
    //fb_esp_firestore_transform_type_increment,
    //fb_esp_firestore_transform_type_maaximum,
    //fb_esp_firestore_transform_type_minimum,
    //fb_esp_firestore_transform_type_append_missing_elements,
    //fb_esp_firestore_transform_typ e_remove_all_from_array
    field_transforms.transform_type = fb_esp_firestore_transform_type_append_missing_elements;

    //For the usage of FirebaseJson, see examples/FirebaseJson/BasicUsage/Create.ino
    FirebaseJson content;



    content.set("values/[0]/stringValue", currentDate);
    content.set("values/[1]/stringValue", timeClient.getFormattedTime());
    content.set("values/[2]/stringValue", String(beatAvg) + "BPM");
    content.set("values/[3]/stringValue", String(ESpO2) + "%");
    //Set the transformation content.
    field_transforms.transform_content = content.raw();

    //Add a field transformation object to a write object.
    transform_write.document_transform.field_transforms.push_back(field_transforms);

    //Add a write object to a write array.
    writes.push_back(transform_write);

    if (Firebase.Firestore.commitDocument(&fbdo, FIREBASE_PROJECT_ID, "" /* databaseId can be (default) or empty */, writes /* dynamic array of fb_esp_firestore_document_write_t */, "" /* transaction */)) {
      Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
      digitalWrite(beep_pin, HIGH);
      delay(500);                       // wait for a second
      digitalWrite(beep_pin, LOW);

    }

    else {
      Serial.println(fbdo.errorReason());
    }

  }
}
