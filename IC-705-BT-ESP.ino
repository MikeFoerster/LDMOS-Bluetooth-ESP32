// BT setup from: https://microcontrollerslab.com/esp32-bluetooth-classic-arduino-ide/


/*
  NOTE: To upload, need to press the button on the ESP32 board once it start to download.   W0IH 9/18/21
  Other settings:  Board: ESP32 DEV Module
                 Flash Mode: QIO
                 Partition Scheme: Default 4MB with spiffs
                 Flash Frequency: 80MHz
                 Upload Speed: 921600
                 Core Debug Level: None
                   and select the correct Comm Port.
     When you see this line starting "Connecting........_", press the button on the board to start the download.
       WORKAROUND! If your ESP32 requires pressing the button, try adding a 10uf capacitor (watch polarity) between the "EN" pin and ground.
          https://randomnerdtutorials.com/solved-failed-to-connect-to-esp32-timed-out-waiting-for-packet-header/
*/

//Version:
// 1.0  Can't seem to get the HC-05, HM-10 or HM-18 to work with the IC-705, so I'm adapting the ESP32 WROOM (which connects and works great)
// 1.6  
//
//  Requirements:
//      The Arduino MEGA Serial3 will connect with the ESP32 Serial2.
//      The CI-V Comms code will reside in the ESP32.
//      Expected Commands: (End of message will be a Carriage Return).  Most 4 Char commands.  ASCII Strings.
//          "freq"  Read the Frequency. Return Freq in KHz.
//          "band"  Return the band as "band[x][xx]".  (Decoded to band similar to MEGA code).  Return as "bnd[0-32]/CR" (i160=0, i80=2, etc...
//                    Frist bit is a flag for HamBand (set to 1 when in the Ham Bands, set to 0 for SWLing)  Used to set Amp Band and Antenna
//          "cbnd"  Return the CurrentBand as "cbnd[x][xx]"
//                    Also has a flag for HamBand (set to 1 when in the Ham Bands, set to 0 for SWLing)  Used to set Amp Band and Antenna
//          "powr"  Return the Power setting of the IC-705, 1 to 10W
//          "powr[1-9]"  Send Power to IC-705, last digit is the power setting.
//          "tnsc"  Read & return the Transceive State
//          "tnsc[0-1]" Change Transceive State to ON or OFF.
//          "poff"   Turn off Power to the IC-705.
//          "slep"   Put the ESP23 to sleep.
//          "vers"   Return 2 digit Version Number
//      In the case of a failure, return "nak/CR", the MEGA will retry.
//
//      Operate in 2 Modes, depending on Transceive Mode:
//        Transceive Mode ON: Continously read: processCatMessages(false).  If a change comes through, decode the LAST (or at least after a pause).
//        Transceive Mode OFF: Respond to "band/CR" command by CIV_Read_Frequency() and decode to band.
//  Version 1.4 works OK.
//     Added code to support SWLing that has 'OffBand' flag to decode Frequency to Band, to allow for SWLing, when OffBand == true, Set Amp Bypass.
//     This version ALSO include test for the new FreqToBand.
//  Version 1.5 Clean out Test for FreqToBand.
//     Changed from "OffBand" to "HamBand" to be compatible with the "Bypass()" function.
//  Version 16 (dropped the decimal point).
//     Added command "vers" for BT Query on startup.  That way the MEGA can display the BT code Version. 




//ESP32 Specific:
#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

//Global to indicate Amp is in Transmit Mode
boolean TxFlag;
const int Version = 16;
void setup()
{
  Serial.begin(115200);
  Serial2.begin(115200);

  SerialBT.begin("IC-705-ESP"); //Bluetooth device name
  //Serial.println("The device started, now you can pair it with bluetooth!");

  //Configure GPIO33 as ext0 (single pin) wake up source for HIGH logic level
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 1);

  //Set the Serial2.readString() timeout (default is 1 second).
  Serial2.setTimeout(25);  //50, 30, 25 appears to work.

  //Setup the TX-Inhibit pin.  High = Transmitting, we don't update the Band during transmit.  Too much chance for error.
  digitalWrite(GPIO_NUM_35, 0);  //Before declaring, make sure it's off.
  pinMode(GPIO_NUM_35, INPUT);

  //Send 'wake' to indicate a wake condition...
  SendCmd("wake");

  Serial.println("Setup");

  // For Testing ESP32 Alone to Monitor
  //  Serial.println(F(" 'freq' Read Frequency,"));
  //  Serial.println(F(" 'band' Reads and Returns Band number,"));
  //  Serial.println(F(" 'cbnd' Returns CurrentBand number,"));
  //  Serial.println(F(" 'powr' Returns Current Power setting, "));
  //  Serial.println(F(" 'powr[1-9]' Sets Power in Watts,"));
  //  Serial.println(F(" 'tnsc' Reads Transceive Value,"));
  //  Serial.println(F(" 'tnsc[0-1]' Sets Transceive State,"));
  //  Serial.println(F(" 'poff' Sets Power Off,"));
  //  Serial.println(F(" 'slep' Puts ESP32 to Sleep."));
  //  Serial.println(F(" 'vers' Return Version Number (2 digit)."));
}

//IC-705 Settings:
// Settings, Bluetooth, Data Device Set, Serialport Function: CI-V(Echo Back OFF)

//ESP32 current requirements on 5v line:  88ma and occasionally bobbles up to 125ma.

void loop() {
  String ReturnString = "";
  //byte Band = 255;
  uint32_t Watts = 0;
  boolean FailTrue = 1;
  uint32_t Freq = 0;
  static uint32_t LastFreq = 0;
  static boolean Transceive;
  static byte CurrentBand;
  static  unsigned long Timer;
  static byte inh;  //TX Inhibit Print Flag
  static boolean HamBand;
  static boolean ReadTrigger;

  TxFlag = digitalRead(GPIO_NUM_35);

  //First Time through, Read the Transceive State.
  if (Timer == 0) {
    //Serial.println(F("Updating Transceive via CIV_Read..."));
    Timer = millis(); //Just using the Timer to tell when we are just starting up.
    //Read the Transceive State and store it in the Transceive static variable.
    Transceive = CIV_ReadTransceiveState();
    ReadTrigger = false;
  }

  if (Serial2.available() > 0) {    // is a character available through Serial2?
    //if (Serial.available() > 0) {    // Test using the Serial Monitor

    unsigned long StartTime = millis();

    String rx_String = Serial2.readString();       // get the character
    String rx_SubString = rx_String.substring(0, 4);
    //Serial.print(F(" rx_String = ")); Serial.print(rx_String); Serial.print(F("  and Substring is: '")); Serial.println(rx_SubString);
    //Serial.print("'"); Serial.print(F("  len=")); Serial.print(rx_String.length()); Serial.println();


    if (rx_SubString == "freq") {
      // Returns Frequency value or "nak"
      //Read the Frequency, return as 3760, 28400, etc.
      Freq = CIV_Read_Frequency();
      //Serial.print(F("OUTCOME: Frequency = ")); Serial.println(Freq); Serial.println();
      if (Freq == 0) ReturnString = "nak";
      else ReturnString = String(Freq);
      //SHOULD I UPDATE the CurrentBand?
    }

    else if (rx_SubString == "band") {
      // Returns CurrentBand value or "nak"
      //Reads the Frequency, decodes the Band and Return the Band (i160, i80, etc.)
      Freq = CIV_Read_Frequency();
      if (Freq <= 255) {
        ReturnString = "nak";
      }
      else {
        CurrentBand = FreqToBand(Freq, HamBand);
        //We also return the "HamBand" flag that changes the Amp Mode to Bypass, because the frequency is Out-Of-Band.
        if (CurrentBand < 10)  ReturnString = String("band" + String(HamBand) + "0" + String(CurrentBand));
        else ReturnString = String("band" + String(HamBand) + String(CurrentBand));
      }
    }

    else if (rx_SubString == "cbnd") {
      // Returns CurrentBand value or "nak"
      //Return the Band (i160, i80, etc.)
      //We also return the "HamBand" flag that changes the Amp Mode to Bypass, because the frequency is Out-Of-Band.
      if (CurrentBand < 10)  ReturnString = "cbnd" + String(HamBand) + "0" + String(CurrentBand);
      else ReturnString = "cbnd" + String(HamBand) + String(CurrentBand);
    }

    else if (rx_SubString == "powr") {
      //Serial.print(F(" powr.length = ")); Serial.println(rx_String.length());
      if (rx_String.length() == 5) {
        //Setting the Power:
        Watts = rx_String.substring(4).toInt();
        //Serial.print("  Setting Watts to: "); Serial.print(Watts);
        FailTrue = CIV_SetPower(Watts);
        if (FailTrue == 1) ReturnString = "nak";
        else ReturnString = "powr" + String(Watts);
      }
      else {
        //Read the power:
        Watts = CIV_Read_RF_Power();
        if (Watts == 0) ReturnString = "nak";
        else ReturnString = "powr" + String(Watts);
      }
    }

    else if (rx_SubString == "tnsc") {
      //Returns Value or "nak"
      //Serial.println(F(" 'tnsc' command received."));
      if (rx_String.length() == 5) {
        //Setting the Transceive Value:
        boolean On_Off = rx_String.substring(4).toInt();
        FailTrue = CIV_Transceive_On_Off(On_Off);
        if (FailTrue) {
          ReturnString = "nak";
          Transceive = false;
        }
        else {
          ReturnString = "tnsc" + String(On_Off);
          Transceive = On_Off;
        }
      }
      else {
        //Read the Transceive setting:
        Transceive = CIV_ReadTransceiveState();  //Also sets the static Transceive variable.
        ReturnString = "tnsc" + String(CurrentBand);
      }
    }

    else if (rx_SubString == "poff") {
      //Turn off the IC-705 Power:
      // Returns "ok"/"nak"
      CIV_Power_Off();
      delay(1000);
      Freq = CIV_Read_Frequency();
      //Serial.print(F("Power OFF Frequency = ")); Serial.println(Freq);
      if (Freq > 255) ReturnString = "nak";
      else ReturnString = "poff1";
    }

    else if (rx_SubString == "vers") {
      //Return Version Number, 2 digit:     
      ReturnString = "vers" + String(Version);
    }
    else if (rx_SubString == "slep") {
      //Put the ESP32 to Deep Sleep.

      // Ref: https://lastminuteengineers.com/esp32-deep-sleep-wakeup-sources/
      // Wake: use 'esp_sleep_enable_ext0_wakeup(GPIO_PIN,LOGIC_LEVEL)' on the D33 Pin.
      //      //Configure GPIO33 as ext0 wake up source for HIGH logic level (3.3v)
      //      esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,1);
      //Because RTC IO module is enabled in this mode, internal pullup or pulldown resistors can also be used.
      //  They need to be configured by the application using rtc_gpio_pullup_en() and rtc_gpio_pulldown_en() functions, before calling esp_sleep_start().

      //Serial.println(F(" 'slep' going to sleep "));
      //Go to sleep now
      esp_deep_sleep_start();
      //We should be asleep, I don't think this will be returned!
      ReturnString = "slep";
    }

    else {
      ReturnString = "InvalidCmd=" + rx_String;
    }


    SendCmd(ReturnString);
    //Serial.print(F("    ReturnString = ")); Serial.println(ReturnString);
    //      Serial.print(F(" Serial2 Comms time = ")); Serial.println(millis() - StartTime);  Serial.println();
  }


  /*
    Outside Comms Loop:
  */

  static byte LastBand;
  if (LastBand != CurrentBand) {
    //Serial.print(F(" Band Update: ")); Serial.println(CurrentBand);
    LastBand = CurrentBand;
  }

  static boolean LastTransceive;
  if (LastTransceive != Transceive) {
    //Serial.print(F(" Transcieve Update: ")); Serial.println(Transceive);
    LastTransceive = Transceive;
  }



  if (Transceive) {
    //Transceive is set to true, keep reading the CI-V for Frequency Change:
    Freq = processCatMessages(false);  //Check the CI-V comms, don't wait for a reply
    if ((Freq > 255) && (Freq != LastFreq)) {
      CurrentBand = FreqToBand(Freq, HamBand);
      LastFreq = Freq;
      //Serial.print(F("  tnsc ON: Freq update = ")); Serial.println(Freq); Serial.print(F("  New CurrentBand update = ")); Serial.println(CurrentBand);
      SendCmd("upd1" + CurrentBand); //The 1 indicates sent with Transceive ON.
    }
  }
  else {
    //Not in the Transcieve Mode...

    //If we are transmitting (TxFlag==1), then reset the Timer so we don't try to read the Band during the transmit.
    if (TxFlag) Timer = millis();

    //Set the ReadTrigger
    if ((millis() - Timer) > 5000) {
      //Update the CurrentBand, poll for the frequency every 5 seconds, to update the CurrentBand variable.
      Freq = CIV_Read_Frequency();
      //Will Return 255 for invalid band:
      CurrentBand = FreqToBand(Freq, HamBand);//Update the CurrentBand variable
      //Serial.print(F("  tnsc OFF: Freq update = ")); Serial.println(Freq); Serial.print(F("  New CurrentBand update = ")); Serial.println(CurrentBand);
      Timer = millis();
    }
    else delay(10);
  }
}

void SendCmd(String Command) {
  for (unsigned int i = 0; i < Command.length(); i++)  {
    Serial2.write(Command[i]);   // Push each char 1 by 1 on each loop pass
  }
}



