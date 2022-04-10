//離線版版本20210705，https://youtu.be/ghTtpUTSc4o
//安裝3個程式庫：1.Adafruit SSD1306、2.MAX30105、3.ESP32Servo
//關於MAX30102可以參閱文件：https://datasheets.maximintegrated.com/en/ds/MAX30102.pdf
//https://pdfserv.maximintegrated.com/en/an/AN6409.pdf

void beep_10ms(){
     digitalWrite(beep_pin,HIGH);
    delay(10);                       // wait for a second
    digitalWrite(beep_pin,LOW);

  }


void max30102_begin() {
  Serial.println("Max30102System Start");
  delay(3000);
  pinMode(beep_pin, OUTPUT);
  //檢查
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("找不到MAX30102");
    while (1);
  }
  byte ledBrightness = 0x7F; //亮度Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only(心跳), 2 = Red + IR(血氧)
  //Options: 1 = IR only, 2 = Red + IR on MH-ET LIVE MAX30102 board
  int sampleRate = 800; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 215; //Options: 69, 118, 215, 411
  int adcRange = 16384; //Options: 2048, 4096, 8192, 16384
  // Set up the wanted parameters
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
  particleSensor.enableDIETEMPRDY();

  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED
}

void max30102_run() {
  long irValue = particleSensor.getIR();    //Reading the IR value it will permit us to know if there's a finger on the sensor or not
  //是否有放手指
  if (irValue > FINGER_ON ) {
    //是否有心跳
    if (checkForBeat(irValue) == true) {
      beep_10ms();
      Serial.print("beatAvg="); Serial.println(beatAvg);//將心跳顯示到序列
      long delta = millis() - lastBeat;//計算心跳差
      lastBeat = millis();
      beatsPerMinute = 60 / (delta / 1000.0);//計算平均心跳
      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        //心跳必須再20-255之間
        rates[rateSpot++] = (byte)beatsPerMinute; //儲存心跳數值陣列
        rateSpot %= RATE_SIZE;
        beatAvg = 0;//計算平均值
        for (byte x = 0 ; x < RATE_SIZE ; x++) beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }else{
        datacount=0;
        }
    }

    //計算血氧
    uint32_t ir, red ;
    double fred, fir;
    particleSensor.check(); //Check the sensor, read up to 3 samples
    if (particleSensor.available()) {
      i++;
      red = particleSensor.getFIFOIR(); //讀取紅光
      ir = particleSensor.getFIFORed(); //讀取紅外線
      //Serial.println("red=" + String(red) + ",IR=" + String(ir) + ",i=" + String(i));
      fred = (double)red;//轉double
      fir = (double)ir;//轉double
      avered = avered * frate + (double)red * (1.0 - frate);//average red level by low pass filter
      aveir = aveir * frate + (double)ir * (1.0 - frate); //average IR level by low pass filter
      sumredrms += (fred - avered) * (fred - avered); //square sum of alternate component of red level
      sumirrms += (fir - aveir) * (fir - aveir);//square sum of alternate component of IR level
      if ((i % Num) == 0) {
        double R = (sqrt(sumredrms) / avered) / (sqrt(sumirrms) / aveir);
        SpO2 = -23.3 * (R - 0.4) + 100;
        ESpO2 = FSpO2 * ESpO2 + (1.0 - FSpO2) * SpO2;//low pass filter
        if (ESpO2 <= MINIMUM_SPO2) ESpO2 = MINIMUM_SPO2; //indicator for finger detached
        if (ESpO2 > 100) ESpO2 = 99.9;
        datacount++;
        Serial.print("Oxygen["); 
        Serial.print(datacount); 
        Serial.print("] =\t"); 
        Serial.print(ESpO2);
        Serial.println("%");
        
        sumredrms = 0.0; sumirrms = 0.0; SpO2 = 0;
        i = 0;
      }
      particleSensor.nextSample(); //We're finished with this sample so move to next sample
    }

  } else {
    //清除心跳數據
    for (byte rx = 0 ; rx < RATE_SIZE ; rx++) rates[rx] = 0;
    beatAvg = 0; rateSpot = 0; lastBeat = 0;
    //清除血氧數據
    avered = 0; aveir = 0; sumirrms = 0; sumredrms = 0;
    SpO2 = 0; ESpO2 = 90.0;
  }
}
