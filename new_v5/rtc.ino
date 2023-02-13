void setupTime()
{
    unsigned long startHere = millis();
    int MM = 0, DD = 0, YY = 0, hh = 0, mm = 0, ss = 0, dd = 0;
    Serial.println(F("\nSet time and date in this format: YYYY,MM,DD,hh,mm,ss,dd[0-6]Mon-Sun"));
    // delay(10);
    while (!Serial.available())
    {
        if (timeOutExit(startHere, DEBUGTIMEOUT))
        {
            debug_flag_exit = true;
            break;
        }
    }
    if (Serial.available())
    {
        YY = Serial.parseInt();
        MM = Serial.parseInt();
        DD = Serial.parseInt();
        hh = Serial.parseInt();
        mm = Serial.parseInt();
        ss = Serial.parseInt();
        dd = Serial.parseInt();
        delay_millis(10);
        adjustDate(YY, MM, DD, hh, mm, ss, dd);
        readTimeStamp();
        Serial.print("Current timestamp: ");
        Serial.println(Ctimestamp);
    }
}

void adjustDate(int year, int month, int date, int hour, int min, int sec, int weekday)
{
    DateTime dt(year, month, date, hour, min, sec, weekday);
    rtc.setDateTime(dt); // adjust date-time as defined by 'dt'
                         // Serial.println(rtc.now().getEpoch());	//debug info
                         //char weekDay[][4] = {"Sun"-0, "Mon"-1, "Tue"-2, "Wed"-3, "Thu"-4, "Fri"-5, "Sat"-6 };
}

float readTemp()
{
    float temp;
    rtc.convertTemperature();
    temp = rtc.getTemperature();
    return temp;
}

char new_temp[15];
char *readTempRTC()
{
    char tmp[10];
    rtc.convertTemperature();
    float temp = rtc.getTemperature();
    snprintf(tmp, sizeof tmp, "%.2f", temp);
    strncpy(new_temp, tmp, sizeof(tmp));
    return new_temp;
}

void readTimeStamp()
{
    DateTime now = rtc.now(); //get the current date-time
    String ts = String(now.year());

    if (now.month() <= 9)
    {
        ts += "0" + String(now.month());
    }
    else
    {
        ts += String(now.month());
    }

    if (now.date() <= 9)
    {
        ts += "0" + String(now.date());
    }
    else
    {
        ts += String(now.date());
    }

    if (now.hour() <= 9)
    {
        ts += "0" + String(now.hour());
    }
    else
    {
        ts += String(now.hour());
    }

    if (now.minute() <= 9)
    {
        ts += "0" + String(now.minute());
    }
    else
    {
        ts += String(now.minute());
    }

    if (now.second() <= 9)
    {
        ts += "0" + String(now.second());
    }
    else
    {
        ts += String(now.second());
    }

    ts.remove(0, 2); //remove 1st 2 data in ts
    ts.toCharArray(Ctimestamp, 13);
}

char *readDateTime()
{
    char storeDt[50];
    storeDt[0] = '\0';

    DateTime now = rtc.now(); //get the current date-time
    String ts = String(now.year());

    if (now.month() <= 9)
    {
        ts += "0" + String(now.month());
    }
    else
    {
        ts += String(now.month());
    }

    if (now.date() <= 9)
    {
        ts += "0" + String(now.date());
    }
    else
    {
        ts += String(now.date());
    }

    if (now.hour() <= 9)
    {
        ts += "0" + String(now.hour());
    }
    else
    {
        ts += String(now.hour());
    }

    if (now.minute() <= 9)
    {
        ts += "0" + String(now.minute());
    }
    else
    {
        ts += String(now.minute());
    }

    if (now.second() <= 9)
    {
        ts += "0" + String(now.second());
    }
    else
    {
        ts += String(now.second());
    }

    ts.remove(0, 2); //remove 1st 2 data in ts
    // ts.toCharArray(Ctimestamp, 13);
    ts.toCharArray(storeDt, 13);

    return storeDt;
}

//default every 10 minutes interval
void setAlarm()
{
    DateTime now = rtc.now(); //get the current date-time

    if ((now.minute() >= 0) && (now.minute() <= 9))
    {
        store_rtc = 10;
    }
    else if ((now.minute() >= 10) && (now.minute() <= 19))
    {
        store_rtc = 20;
    }
    else if ((now.minute() >= 20) && (now.minute() <= 29))
    {
        store_rtc = 30;
    }
    else if ((now.minute() >= 30) && (now.minute() <= 39))
    {
        store_rtc = 40;
    }
    else if ((now.minute() >= 40) && (now.minute() <= 49))
    {
        store_rtc = 50;
    }
    else if ((now.minute() >= 50) && (now.minute() <= 59))
    {
        store_rtc = 0;
    }
    rtc.enableInterrupts(store_rtc, 00); // interrupt at (m,s)
    if (DEBUG == 1)
    {
        Serial.print("Next alarm: ");
    }
    if (DEBUG == 1)
    {
        Serial.println(store_rtc);
    }
}

//30 minutes interval
void setAlarmEvery30(int alarmSET)
{
    DateTime now = rtc.now(); //get the current date-time
    switch (alarmSET)
    {
    case 0:
    {
        // Serial.println("set 0/30 . . .");
        if ((now.minute() >= 0) && (now.minute() <= 29))
        {
            store_rtc = 30;
        }
        else if ((now.minute() >= 30) && (now.minute() <= 59))
        {
            store_rtc = 0;
        }
        enable_rtc_interrupt();
        break;
    }
    case 1:
    {
        // Serial.println("set 5/35 . . .");
        if ((now.minute() >= 0) && (now.minute() <= 34))
        {
            store_rtc = 35;
        }
        else if ((now.minute() >= 35) && (now.minute() <= 59))
        {
            store_rtc = 5;
        }
        enable_rtc_interrupt();
        break;
    }
    case 2:
    {
        // Serial.println("set 10/40 . . .");
        if ((now.minute() >= 0) && (now.minute() <= 39))
        {
            store_rtc = 40;
        }
        else if ((now.minute() >= 40) && (now.minute() <= 59))
        {
            store_rtc = 10;
        }
        enable_rtc_interrupt();
        break;
    }
    case 3:
    {
        // Serial.println("set 15/45 . . .");
        if ((now.minute() >= 0) && (now.minute() <= 44))
        {
            store_rtc = 45;
        }
        else if ((now.minute() >= 45) && (now.minute() <= 59))
        {
            store_rtc = 15;
        }
        enable_rtc_interrupt();
        break;
    }
    case 4:
    {
        //set every 10 minutes interval
        if ((now.minute() >= 0) && (now.minute() <= 9))
        {
            store_rtc = 10;
        }
        else if ((now.minute() >= 10) && (now.minute() <= 19))
        {
            store_rtc = 20;
        }
        else if ((now.minute() >= 20) && (now.minute() <= 29))
        {
            store_rtc = 30;
        }
        else if ((now.minute() >= 30) && (now.minute() <= 39))
        {
            store_rtc = 40;
        }
        else if ((now.minute() >= 40) && (now.minute() <= 49))
        {
            store_rtc = 50;
        }
        else if ((now.minute() >= 50) && (now.minute() <= 59))
        {
            store_rtc = 0;
        }
        enable_rtc_interrupt();
        break;
    }
    case 5:
    {
        //set every 15 minutes interval
        if ((now.minute() >= 5) && (now.minute() <= 14))
        {
            store_rtc = 15;
        }
        else if ((now.minute() >= 15) && (now.minute() <= 24))
        {
            store_rtc = 25;
        }
        else if ((now.minute() >= 25) && (now.minute() <= 34))
        {
            store_rtc = 35;
        }
        else if ((now.minute() >= 35) && (now.minute() <= 44))
        {
            store_rtc = 45;
        }
        else if ((now.minute() >= 45) && (now.minute() <= 54))
        {
            store_rtc = 55;
        }
        else if ((now.minute() >= 55) && (now.minute() <= 4))
        {
            store_rtc = 5;
        }
        enable_rtc_interrupt();
        break;
    }
    case 6:
    {
        //set every 10 minutes interval
        if ((now.minute() >= 0) && (now.minute() <= 9))
        {
            store_rtc = 10;
        }
        else if ((now.minute() >= 10) && (now.minute() <= 19))
        {
            store_rtc = 20;
        }
        else if ((now.minute() >= 20) && (now.minute() <= 29))
        {
            store_rtc = 30;
        }
        else if ((now.minute() >= 30) && (now.minute() <= 39))
        {
            store_rtc = 40;
        }
        else if ((now.minute() >= 40) && (now.minute() <= 49))
        {
            store_rtc = 50;
        }
        else if ((now.minute() >= 50) && (now.minute() <= 59))
        {
            store_rtc = 0;
        }
        enable_rtc_interrupt();
        break;
    }
    case 7: //5 mins interval
    {
        if ((now.minute() >= 0) && (now.minute() <= 4)) {
            store_rtc = 5;
        } else if ((now.minute() >= 5) && (now.minute() <= 9)) {
            store_rtc = 10;
        } else if ((now.minute() >= 10) && (now.minute() <= 14)) {
            store_rtc = 15;
        } else if ((now.minute() >= 15) && (now.minute() <= 19)) {
            store_rtc = 20;
        } else if ((now.minute() >= 20) && (now.minute() <= 24)) {
            store_rtc = 25;
        } else if ((now.minute() >= 25) && (now.minute() <= 29)) {
            store_rtc = 30;
        } else if ((now.minute() >= 30) && (now.minute() <= 34)) {
            store_rtc = 35;
        } else if ((now.minute() >= 35) && (now.minute() <= 39)) {
            store_rtc = 40;
        } else if ((now.minute() >= 40) && (now.minute() <= 44)) {
            store_rtc = 45;
        } else if ((now.minute() >= 45) && (now.minute() <= 49)) {
            store_rtc = 50;
        } else if ((now.minute() >= 50) && (now.minute() <= 54)) {
            store_rtc = 55;
        } else if ((now.minute() >= 55) && (now.minute() <= 0)) {
            store_rtc = 0;
        }
        enable_rtc_interrupt();
        break;
    }
    }
}

void enable_rtc_interrupt()
{
    rtc.enableInterrupts(store_rtc, 00); // interrupt at (minutes, seconds)
    if (DEBUG == 1)
    {
        Serial.print("Next alarm: ");
    }
    if (DEBUG == 1)
    {
        Serial.println(store_rtc);
    }
    readTimeStamp();
}
