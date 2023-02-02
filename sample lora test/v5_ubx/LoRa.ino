void init_lora()
{
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

    // manual reset
    digitalWrite(RFM95_RST, LOW);
    delay_millis(10);
    digitalWrite(RFM95_RST, HIGH);
    delay_millis(10);

    while (!rf95.init())
    {
        Serial.println("LoRa radio init failed");
        while (1);
    }
    Serial.println("LoRa radio init OK!");

    // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
    if (!rf95.setFrequency(RF95_FREQ))
    {
        Serial.println("setFrequency failed");
        while (1);
    }
    Serial.print("Set Freq to: ");
    Serial.println(RF95_FREQ);
    /**
     *  Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
     * The default transmitter power is 13dBm, using PA_BOOST.
     * If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
     * you can set transmitter powers from 5 to 23 dBm:
     */
    rf95.setTxPower(23, false);
}

void send_thru_lora(char *radiopacket)
{
    int length = sizeof(payload);
    int i = 0, j = 0;
    bool valid_msg = false;
    int LORA_SEND_RETRY_LIMIT = 3;

    if (get_logger_version() == 10)
    {
        for (j = 0; j <= LORA_SEND_RETRY_LIMIT; j++)
        {
            // do not stack
            for (i = 0; i < 200; i++)
            {
                payload[i] = (uint8_t)'0';
            }
            for (i = 0; i < length; i++)
            {
                payload[i] = (uint8_t)radiopacket[i];
            }
            payload[i] = (uint8_t)'\0';
            Serial.print("Sending to LoRa: ");
            Serial.println((char *)payload);
            // Serial.println("sending payload!");
            rf95.send(payload, length);               // sending data to LoRa
            rf95.waitPacketSent();                    // Waiting for packet to complete...
            Serial.println("Waiting for reply...");
            if (rf95.waitAvailableTimeout(1000))
            {
                // Should be a reply message for us now
                if (rf95.recv(payload, &len))
                {
                    Serial.print("Got reply: ");
                    Serial.println((char *)payload);
                    Serial.print("RSSI: ");
                    Serial.println(rf95.lastRssi(), DEC);

                    if (ack_LoRa_reply((char *)payload))
                    {
                        break;
                    }
                }
                else
                {
                    Serial.println("Receive failed");
                }
            }
            else
            {
                Serial.println("No reply, is there a listener around?");
                Serial.println(" ");
            }
            Serial.println(j);
        }
    }
    else
    {
        // do not stack
        for (i = 0; i < 200; i++)
        {
            payload[i] = (uint8_t)'0';
        }
        for (i = 0; i < length; i++)
        {
            payload[i] = (uint8_t)radiopacket[i];
        }
        payload[i] = (uint8_t)'\0';
        Serial.print("Sending to LoRa: ");
        Serial.println((char *)payload);
      // Serial.println("sending payload!");
        rf95.send(payload, length);           // sending data to LoRa
        delay_millis(100);
    }
}

bool ack_LoRa_reply(char *LoRa_reply)
{
    char temp_reply[50];
    char _ack[8] = "^REC'D_";

    strncpy(temp_reply, _ack, sizeof(_ack));
    strncat(temp_reply, get_logger_B_from_flashMem(), sizeof(get_logger_B_from_flashMem()));
    Serial.println(F(temp_reply));

    if (strstr(LoRa_reply, temp_reply))
    {
        Serial.println(F("Valid ACK Received!"));
        return true;
    }
    else
    {
        Serial.println(F("Invalid ACK!"));
        return false;
    }
}

void receive_lora_data(uint8_t mode)
{
    disable_watchdog();
    int count = 0;
    int count2 = 0;
    unsigned long start = millis();
    Serial.println("waiting for LoRa data . . .");
    while (rcv_LoRa_flag == 0)
    {
        if (mode == 4)
        {
            // timeOut in case walang makuhang data LoRa transmitter ~15 minutes
            if ((millis() - start) > LORATIMEOUTMODE2)
            {
                start = millis();
                // send gateway rssi values if no received from transmitter
                get_rssi(get_logger_version());
                rcv_LoRa_flag = 1;
            }
        }
        else if (mode == 5 || mode == 15)
        {
            // timeOut in case walang makuhang data LoRa transmitter ~15 minutes
            if ((millis() - start) > LORATIMEOUTMODE3)
            {
                start = millis();
                // send gateway rssi values if no received from transmitter
                get_rssi(get_logger_version());
                rcv_LoRa_flag = 1;
            }
        }
        else if (mode == 13)
        {
            // timeOut in case walang makuhang data LoRa transmitter ~4 minutes 260 000
            if ((millis() - start) > 360000)
            {
                Serial.println("Time out reached for Surficial Tilt Gateway");
                start = millis();
                send_thru_gsm("Time out . . .", get_serverNum_from_flashMem());
                rcv_LoRa_flag = 1;
            }
        }
        else if ( mode == 1)
        {
            // timeOut in case walang makuhang data LoRa transmitter ~4 minutes 260 000
            if ((millis() - start) > LORATIMEOUT)
            {
                Serial.println("Time out reached.");
                start = millis();
                get_rssi(4);
                rcv_LoRa_flag = 1;
            }
        }
        else
        {
            // timeOut in case walang makuhang data LoRa transmitter ~4 minutes 260 000
            if ((millis() - start) > LORATIMEOUT)
            {
                Serial.println("Time out reached.");
                start = millis();
                get_rssi(get_logger_version());
                rcv_LoRa_flag = 1;
            }
        }

        if (rf95.available())
        {
            // Should be a message for us now
            if (rf95.recv(buf, &len2))
            {
                int i = 0;
                for (i = 0; i < len2; ++i)
                {
                    received[i] = (uint8_t)buf[i];
                }
                received[i] = (uint8_t)'\0';

                if (strstr(received, ">>"))
                { /*NOT LoRa: 0, 2, 6, 7*/
                    flashLed(LED_BUILTIN, 3, 60);
                    if (mode == 1 || mode == 3 || mode == 4 || mode == 5 || mode == 13 || mode == 15) // LoRa mode only
                    {
                        /*remove 1st & 2nd character*/
                        for (byte i = 0; i < strlen(received); i++)
                        {
                            received[i] = received[i + 2];
                        }
                        // surficial tilt logger
                        if (mode == 15)
                        {
                            Serial.print("Surficial Tilt Received: ");
                            Serial.println(received);
                            if_receive_valid(received);
                            
                            if (valid_LoRa_tx)
                            {
                                send_LoRa_reply(if_receive_valid(received));
                                send_thru_gsm(received, get_serverNum_from_flashMem());
                            }
                            else
                            {
                                Serial.println("InValid transmitter!");
                            }
                        }
                        else
                        {
                            send_thru_gsm(received, get_serverNum_from_flashMem());

                            // print RSSI values
                            tx_RSSI = String(rf95.lastRssi(), DEC);
                            Serial.print("RSSI: ");
                            Serial.println(tx_RSSI);
                        }
                    }
                    else
                    {
                        Serial.print("Received Data: ");
                        Serial.println(received);
                        // print RSSI values
                        tx_RSSI = String(rf95.lastRssi(), DEC);
                        Serial.print("RSSI: ");
                        Serial.println(tx_RSSI);
                    }
                }
                /*
                else if (received, "STOPLORA")
                {
                // function is working as of 03-12-2020
                count++;
                if( mode == 4)
                {
                    Serial.println("Version 4: STOPLORA");
                    if (count >= 2)
                    {
                    Serial.print("count: ");
                    Serial.println(count);
                    Serial.println("Recieved STOP LoRa.");
                    count = 0;
                    }
                }
                else
                {
                    Serial.print("count: ");
                    Serial.println(count);
                    Serial.println("Recieved STOP LoRa.");
                    count = 0;
                    // rcv_LoRa_flag = 1;
                }
                }
                */
                else if (received, "*VOLT:")
                {
                    if (mode == 4)        // 2 LoRa transmitter
                    {
                        count2++;
                        Serial.print("recieved counter: ");
                        Serial.println(count2);

                        if (count2 == 1)
                        {
                            // SENSOR A
                            tx_RSSI = String(rf95.lastRssi(), DEC);
                            Serial.print("RSSI: ");
                            Serial.println(tx_RSSI);                                            //  parse voltage, MADTB*VOLT:12.33*200214111000
                            parse_voltage(received).toCharArray(txVoltage, sizeof(txVoltage));
                            Serial.print("TX Voltage A: ");
                            Serial.println(txVoltage);
                        }
                        else if (count2 == 2)
                        {
                            // SENSOR B
                            tx_RSSI_B = String(rf95.lastRssi(), DEC);
                            Serial.print("RSSI: ");
                            Serial.println(tx_RSSI_B);
                            parse_voltage(received).toCharArray(txVoltageB, sizeof(txVoltageB));
                            Serial.print("TX Voltage B: ");
                            Serial.println(txVoltageB);
                            delay_millis(500);
                            get_rssi(get_logger_version());
                            count2 = 0;
                            rcv_LoRa_flag = 1;
                        }
                    }
                    //else if (mode == 5 || mode == 12) // 3 LoRa transmitter
                    else if (mode == 5)                 // 3 LoRa transmitter
                    {
                        count2++;
                        Serial.print("counter: ");
                        Serial.println(count2);

                        if (count2 == 1)
                        {
                            // SENSOR A
                            tx_RSSI = String(rf95.lastRssi(), DEC);
                            Serial.print("RSSI: ");
                            Serial.println(tx_RSSI);
                            // parse voltage, MADTB*VOLT:12.33*200214111000
                            parse_voltage(received).toCharArray(txVoltage, sizeof(txVoltage));
                            Serial.print("TX Voltage A: ");
                            Serial.println(txVoltage);
                        }
                        else if (count2 == 2)
                        {
                            // SENSOR B
                            tx_RSSI_B = String(rf95.lastRssi(), DEC);
                            Serial.print("RSSI: ");
                            Serial.println(tx_RSSI);
                            //  parse voltage, MADTB*VOLT:12.33*200214111000
                            parse_voltage(received).toCharArray(txVoltageB, sizeof(txVoltageB));
                            Serial.print("TX Voltage B: ");
                            Serial.println(txVoltageB);
                        }
                        else if (count2 == 3)
                        {
                            // SENSOR C
                            tx_RSSI_C = String(rf95.lastRssi(), DEC);
                            Serial.print("RSSI: ");
                            Serial.println(tx_RSSI_B);
                            parse_voltage(received).toCharArray(txVoltageC, sizeof(txVoltageC));
                            Serial.print("TX Voltage C: ");
                            Serial.println(txVoltageC);
                            get_rssi(get_logger_version());
                            count2 = 0;
                            rcv_LoRa_flag = 1;
                        }
                    }
                    else
                    {
                        /*only 1 transmitter*/
                        tx_RSSI = String(rf95.lastRssi(), DEC);
                        Serial.print("RSSI: ");
                        Serial.println(tx_RSSI);                        //  parse voltage, MADTB*VOLT:12.33*200214111000
                        parse_voltage(received).toCharArray(txVoltage, sizeof(txVoltage));
                        Serial.print("Received Voltage: ");
                        Serial.println(parse_voltage(received));
                        get_rssi(get_logger_version());
                        rcv_LoRa_flag = 1;
                    }
                }
            }
            else
            {
                Serial.println("Invalid LoRa message!");
                // rcv_LoRa_flag = 1;
            }
        }
    } // while (rcv_LoRa_flag == 0); //if NOT same with condition Loop will exit

    count = 0;
    count2 = 0;
    rcv_LoRa_flag = 0;
    getSensorDataFlag = false;
    valid_LoRa_tx = false;
    txVoltage[0] = '\0';
    txVoltageB[0] = '\0';
    txVoltageC[0] = '\0';
    flashLed(LED_BUILTIN, 3, 80);
    enable_watchdog();
}

void receive_ublox_data(uint8_t mode){
    rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
    disable_watchdog();
    digitalWrite(LED_BUILTIN, HIGH);
    int count = 0;
    int count2 = 0;
    bool timeoutFlag = false;
    bool ack_receive = false;
    char ACK[] = "ACK";
    char ND[] = "No UBLOX data.";
    unsigned long start = millis();
    Serial.println("waiting for UBLOX data (5mins) . . .");

    while ((rcv_LoRa_flag == 0) && (timeoutFlag == false)  )
    {
        if ((millis() - start ) >= 180000 ){
            timeoutFlag = true;
        }
         if (rf95.available())
         {
            if (rf95.recv(buf, &len2))
            {
                int i = 0;
                for (i = 0; i < len2; ++i)
                {
                    received[i] = (uint8_t)buf[i];
                }
                received[i] = (uint8_t)'\0';
                Serial.println(received);


                if (mode == 4) { //2 lora tx
                    count2++;
                    Serial.print("received counter: ");
                    Serial.println(count2);
  
                    if (count2 == 1) { //1st tx
                        if (strstr(received, "TESUA") || strstr(received, "TESUB")){
                            readTimeStamp();
                            Serial.print("-->>");
                            Serial.println(received);
                            strncat(received,"*",1);
                            strncat(received,Ctimestamp,13);
                            send_thru_gsm(received, get_serverNum_from_flashMem());
                            rcv_LoRa_flag = 1;
                        }
                        
                    else if (count2 ==2) {
                        if (strstr(received, "TESUA") || strstr(received, "TESUB")){
                            readTimeStamp();
                            Serial.print("-->>");
                            Serial.println(received);
                            strncat(received,"*",1);
                            strncat(received,Ctimestamp,13);
                            send_thru_gsm(received, get_serverNum_from_flashMem());
                            rcv_LoRa_flag = 1;
                        }
                    }
                }
                                
//                if (strstr(received, "TESUA")){
//                    readTimeStamp();
//                    Serial.print("-->>");
//                    Serial.println(received);
//                    strncat(received,"*",1);
//                    strncat(received,Ctimestamp,13);
//                    send_thru_gsm(received, get_serverNum_from_flashMem());
//                    rcv_LoRa_flag = 1;
                } else {
                    Serial.println("not ublox");
                }
             }
        }
    } 
    if (timeoutFlag){
        readTimeStamp();
        strncat(received,"*",1);
        strncat(received,Ctimestamp,13);
        send_thru_gsm(ND, get_serverNum_from_flashMem());
    }
    
    rcv_LoRa_flag = 0;
    getSensorDataFlag = false;
    valid_LoRa_tx = false;
    txVoltage[0] = '\0';
    txVoltageB[0] = '\0';
    txVoltageC[0] = '\0';
    digitalWrite(LED_BUILTIN, HIGH);
    flashLed(LED_BUILTIN, 3, 80);
    enable_watchdog();
}

////////////RECEIVE UBLOX DATA --v2
void receive_ublox_data_v2(uint8_t mode, char* ublox_data) {
    rf95.setModemConfig(RH_RF95::Bw125Cr45Sf128);
    disable_watchdog();
    digitalWrite(LED_BUILTIN, HIGH);
    int count = 0;
    int count2 = 0;
    bool timeoutFlag = false;
    bool ack_receive = false;
    char ACK[] = "ACK";
    char ND[] = "No UBLOX data.";
    unsigned long start = millis();
    Serial.println("waiting for UBLOX data (5mins) . . .");

    while ((rcv_LoRa_flag == 0) && (timeoutFlag == false)  )
    {
        if ((millis() - start ) >= 300000 ){
            timeoutFlag = true;
        }
        // if (rf95.available())
        // {
            if (rf95.recv(buf, &len2))
            {
                int i = 0;
                for (i = 0; i < len2; ++i)
                {
                    received[i] = (uint8_t)buf[i];
                }
                received[i] = (uint8_t)'\0';
                Serial.println(received);
                if (strstr(received, (char*)ublox_data)){
                    //send_thru_lora(ACK);
                    // //ack here
                    // for (i = 0; i < sizeof(ACK); i++)
                    // {
                    //     payload[i] = (uint8_t)ACK[i];
                    // }
                    // payload[i] = (uint8_t)'\0';
                    // Serial.println((char*)payload);
                    // rf95.send(payload,sizeof(payload));
                    // rf95.waitPacketSent(); 
                    //Serial.println("-->>ACK sent!");

                    readTimeStamp();
                    Serial.print("-->>");
                    Serial.println(received);
                    strncat(received,"*",1);
                    strncat(received,Ctimestamp,13);
                    send_thru_gsm(received, get_serverNum_from_flashMem());
                    rcv_LoRa_flag = 1;
                } else {
                    Serial.println("not ublox");
                }
            // }

        }
    } 
    if (timeoutFlag)
    {
        readTimeStamp();
        strncat(received,"*",1);
        strncat(received,Ctimestamp,13);
        send_thru_gsm(ND, get_serverNum_from_flashMem());
    }
    
    rcv_LoRa_flag = 0;
    getSensorDataFlag = false;
    valid_LoRa_tx = false;
    txVoltage[0] = '\0';
    txVoltageB[0] = '\0';
    txVoltageC[0] = '\0';
    digitalWrite(LED_BUILTIN, HIGH);
    flashLed(LED_BUILTIN, 3, 80);
    enable_watchdog();
}

void send_LoRa_reply(uint8_t _valid_tx)
{
    char _reply_to_sent[20];
    char _keyword[8] = "^REC'D_"; // keyword to send to the transmitter once valid data received
    Serial.println(_valid_tx);

    strncpy(_reply_to_sent, _keyword, sizeof(_keyword));
    // strncat(_reply_to_sent, _valid_tx, sizeof(_valid_tx) + 1);
    if (_valid_tx == 1)
    {
        strncat(_reply_to_sent, get_logger_B_from_flashMem(), sizeof(get_logger_B_from_flashMem()) + 5);
    }
    else if (_valid_tx == 2)
    {
        strncat(_reply_to_sent, get_logger_C_from_flashMem(), sizeof(get_logger_C_from_flashMem()) + 5);
    }
    else if (_valid_tx == 3)
    {
        strncat(_reply_to_sent, get_logger_D_from_flashMem(), sizeof(get_logger_D_from_flashMem()) + 5);
    }
    else if (_valid_tx == 4)
    {
        strncat(_reply_to_sent, get_logger_E_from_flashMem(), sizeof(get_logger_E_from_flashMem()) + 5);
    }
    else if (_valid_tx == 5)
    {
        strncat(_reply_to_sent, get_logger_F_from_flashMem(), sizeof(get_logger_F_from_flashMem()) + 5);
    }
    Serial.print("Sent reply: ");
    Serial.println(_reply_to_sent);

    send_thru_lora(_reply_to_sent);
    /*
  uint8_t data[] = "^REC'D_MADTB";
  rf95.send(data, sizeof(data));
  rf95.waitPacketSent();
  Serial.print("Sent reply: ");
  Serial.println((char *)data);
  */
}

uint8_t if_receive_valid(char *_received)
{
    uint8_t _tx_name = 0;
    char _Blog[20];
    char _Clog[20];
    char _Dlog[20];
    char _Elog[20];
    char _Flog[20];
    String _loggerB = String(get_logger_B_from_flashMem());
    String _loggerC = String(get_logger_C_from_flashMem());
    String _loggerD = String(get_logger_D_from_flashMem());
    String _loggerE = String(get_logger_E_from_flashMem());
    String _loggerF = String(get_logger_F_from_flashMem());

    _loggerB.toCharArray(_Blog, _loggerB.length());
    _loggerC.toCharArray(_Clog, _loggerC.length());
    _loggerD.toCharArray(_Dlog, _loggerD.length());
    _loggerE.toCharArray(_Dlog, _loggerE.length());
    _loggerF.toCharArray(_Dlog, _loggerF.length());

    if (strstr(_received, _Blog))
    {
        valid_LoRa_tx = true;
        _tx_name = 1;
        // Serial.println("Valid LoRa transmitter!");
        // Serial.println(_Blog);
        // Serial.println(_received);
        return _tx_name;
    }
    else if (strstr(_received, _Clog))
    {
        valid_LoRa_tx = true;
        _tx_name = 2;
        // Serial.println("Valid LoRa transmitter!");
        // Serial.println(_Clog);
        // Serial.println(_received);
        return _tx_name;
    }
    else if (strstr(_received, _Dlog))
    {
        valid_LoRa_tx = true;
        _tx_name = 3;
        // Serial.println("Valid LoRa transmitter!");
        // Serial.println(_Dlog);
        // Serial.println(_received);
        return _tx_name;
    }

    else if (strstr(_received, _Elog))
    {
        valid_LoRa_tx = true;
        _tx_name = 4;
        // Serial.println("Valid LoRa transmitter!");
        // Serial.println(_Dlog);
        // Serial.println(_received);
        return _tx_name;
    }
    else if (strstr(_received, _Flog))
    {
        valid_LoRa_tx = true;
        _tx_name = 5;
        // Serial.println("Valid LoRa transmitter!");
        // Serial.println(_Dlog);
        // Serial.println(_received);
        return _tx_name;
    }
    else
    {
        valid_LoRa_tx = false;
        Serial.println("INVALID LoRa transmitter!");
        return _tx_name;
    }
}

void receive_lora_data_UBLOX(uint8_t mode)
{
    disable_watchdog();
    int count = 0;
    int count2 = 0;
    unsigned long start = millis();
    Serial.println("waiting for LoRa data . . .");
    while (rcv_LoRa_flag == 0)
    {
        uint8_t buf2[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len3 = sizeof(buf2);
        if (mode == 4)
        {
            // timeOut in case walang makuhang data LoRa transmitter ~15 minutes
            if ((millis() - start) > LORATIMEOUTMODE2)
            {
                start = millis();
                // send gateway rssi values if no received from transmitter
                get_rssi(get_logger_version());
                rcv_LoRa_flag = 1;
            }
        }
        else if (mode == 5)
        {
            // timeOut in case walang makuhang data LoRa transmitter ~15 minutes
            if ((millis() - start) > LORATIMEOUTMODE3)
            {
                start = millis();
                // send gateway rssi values if no received from transmitter
                get_rssi(get_logger_version());
                rcv_LoRa_flag = 1;
            }
        }
        else if (mode == 13)
        {
            // timeOut in case walang makuhang data LoRa transmitter ~4 minutes 260 000
            if ((millis() - start) > LORATIMEOUTMODE2)
            {
                Serial.println("Time out reached for Surficial Tilt Gateway");
                start = millis();
                send_thru_gsm("Time out . . .", get_serverNum_from_flashMem());
                rcv_LoRa_flag = 1;
            }
        }
        else
        {
            // timeOut in case walang makuhang data LoRa transmitter ~4 minutes 260 000
            if ((millis() - start) > LORATIMEOUT)
            {
                Serial.println("Time out reached.");
                start = millis();
                get_rssi(get_logger_version());
                rcv_LoRa_flag = 1;
            }
        }

        if (rf95.available())
        {
            // Should be a message for us now
            if (rf95.recv(buf2, &len3))
            {
                int i = 0;
                for (i = 0; i < len3; ++i)
                {
                    received[i] = (uint8_t)buf2[i];
                }
                received[i] = (uint8_t)'\0';

                if (strstr(received, ">>"))
                { /*NOT LoRa: 0, 2, 6, 7*/
                    flashLed(LED_BUILTIN, 3, 60);
                    if (mode == 1 || mode == 3 || mode == 4 || mode == 5 || mode == 13 || mode == 15) // LoRa mode only
                    {
                        /*remove 1st & 2nd character*/
                        for (byte i = 0; i < strlen(received); i++)
                        {
                            received[i] = received[i + 2];
                        }
                        // surficial tilt logger
                        if (mode == 15)
                        {
                            Serial.print("Surficial Tilt Received: ");
                            Serial.println(received);

                            if_receive_valid(received);
                            if (valid_LoRa_tx)
                            {
                                send_LoRa_reply(if_receive_valid(received));

                                send_thru_gsm(received, get_serverNum_from_flashMem());
                            }
                            else
                            {
                                Serial.println("InValid transmitter!");
                            }
                        }
                        else
                        {
                            send_thru_gsm(received, get_serverNum_from_flashMem());

                            // print RSSI values
                            tx_RSSI = String(rf95.lastRssi(), DEC);
                            Serial.print("RSSI: ");
                            Serial.println(tx_RSSI);
                        }
                    }
                    else
                    {
                        Serial.print("Received Data: ");
                        Serial.println(received);
                        // print RSSI values
                        tx_RSSI = String(rf95.lastRssi(), DEC);
                        Serial.print("RSSI: ");
                        Serial.println(tx_RSSI);
                    }
                }
                else
                {
                    Serial.println("Invalid LoRa message!");
                    // rcv_LoRa_flag = 1;
                }
            }
        }
    }

    count = 0;
    count2 = 0;
    rcv_LoRa_flag = 0;
    getSensorDataFlag = false;
    valid_LoRa_tx = false;
    txVoltage[0] = '\0';
    txVoltageB[0] = '\0';
    txVoltageC[0] = '\0';
    flashLed(LED_BUILTIN, 3, 80);
    enable_watchdog();
}

/**
 * @brief LoRa recieve only
 *
 * @param mode
 */

void receive_lora_data_ONLY(uint8_t mode)
{
    disable_watchdog();
    int count = 0;
    int count2 = 0;
    unsigned long start = millis();
    Serial.println("waiting for LoRa data . . .");
    while (rcv_LoRa_flag == 0)
    {
        uint8_t buf2[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t len3 = sizeof(buf2);
        if (mode == 14)
        {
            // timeOut in case walang makuhang data LoRa transmitter ~15 minutes
            if ((millis() - start) > LORATIMEOUTMODE3)
            {
                start = millis();
                // send gateway rssi values if no received from transmitter
                get_rssi(get_logger_version());
                rcv_LoRa_flag = 1;
            }
        }
        else
        {
            // timeOut in case walang makuhang data LoRa transmitter ~4 minutes 260 000
            if ((millis() - start) > LORATIMEOUT)
            {
                Serial.println("Time out reached.");
                start = millis();
                get_rssi(get_logger_version());
                rcv_LoRa_flag = 1;
            }
        }

        if (rf95.available())
        {
            // Should be a message for us now
            if (rf95.recv(buf2, &len3))
            {
                int i = 0;
                for (i = 0; i < len3; ++i)
                {
                    received[i] = (uint8_t)buf2[i];
                }
                received[i] = (uint8_t)'\0';

                if (strstr(received, ">>"))
                { /*NOT LoRa: 0, 2, 6, 7*/
                    flashLed(LED_BUILTIN, 3, 60);
                    if (mode == 1 || mode == 3 || mode == 4 || mode == 5 || mode == 14) // LoRa mode only
                    {
                        /*remove 1st & 2nd character*/
                        for (byte i = 0; i < strlen(received); i++)
                        {
                            received[i] = received[i + 2];
                        }

                        send_thru_gsm(received, get_serverNum_from_flashMem());
                        // print RSSI values
                        tx_RSSI = String(rf95.lastRssi(), DEC);
                        Serial.print("RSSI: ");
                        Serial.println(tx_RSSI);
                    }
                    else
                    {
                        Serial.print("Received Data: ");
                        Serial.println(received);
                        // print RSSI values
                        tx_RSSI = String(rf95.lastRssi(), DEC);
                        Serial.print("RSSI: ");
                        Serial.println(tx_RSSI);
                    }
                }
                else if (received, "*VOLT:")
                {
                    send_thru_gsm(received, get_serverNum_from_flashMem());

                    tx_RSSI = String(rf95.lastRssi(), DEC);
                    Serial.print("RSSI: ");
                    Serial.println(tx_RSSI);
                }
                else
                {
                    Serial.println("Invalid LoRa message!");
                    // rcv_LoRa_flag = 1;
                }
            }
        }
    }
    rcv_LoRa_flag = 0;
    flashLed(LED_BUILTIN, 3, 80);
    enable_watchdog();
}
