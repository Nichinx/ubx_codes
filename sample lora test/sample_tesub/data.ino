float readBatteryVoltage(uint8_t ver) {
  float measuredvbat;
  if ((ver == 3) || (ver == 9) || (ver == 10) || (ver == 11)) {
    measuredvbat = analogRead(VBATPIN); //Measure the battery voltage at pin A7
    measuredvbat *= 2;                  // we divided by 2, so multiply back
    measuredvbat *= 3.3;                // Multiply by 3.3V, our reference voltage
    measuredvbat /= 1024;               // convert to voltage
    measuredvbat += 0.28;               // add 0.7V drop in schottky diode
  } else {
    /* Voltage Divider 1M and  100k */
    measuredvbat = analogRead(VBATEXT);
    measuredvbat *= 3.3;                // reference voltage
    measuredvbat /= 1024.0;             // adc max count
    measuredvbat *= 11.0;               // (100k+1M)/100k
  }
  return measuredvbat;
}

void read_data(){
    for (int i = 0; i < 200; i++) {
      dataToSend[i] = 0x00;  
    }
    
    char tempstr[100];
    readTimeStamp();

    sprintf(tempstr, "TESUB: is the other string; 2nd data.");
    strncat(dataToSend, tempstr, String(tempstr).length() + 1);
    strncat(dataToSend, Ctimestamp, sizeof(Ctimestamp));       
    Serial.print("data to send: ");
    Serial.println(dataToSend);  
}
