//NOTE: Set, Bluetooth, Data Device Set, Serialport Function must be set to "CI-V (Echo Back OFF).  (Can't find the command to turn it off!

// Basic code Copied from https://github.com/ok1cdj/IC705-BT-CIV

/*  ICOM CIV code is from https://github.com/darkbyte-ru/ICOM-CI-V-cat
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Notes:
// http://www.plicht.de/ekki/civ/civ-p3.html
// https://kb3hha.com/ArticleDetails?id=6


//NOTE: For ESP32: To upload, need to press the button on the ESP32 board once it start to download.   W0IH 9/18/21
//Other settings:  Board: ESP Wrover Module, Upload Speed: 115200, Flash Freq: 80MHz, Flash Mode: QIO, Partition: 4MB, Core Debug Level: None,

/*What I've learned...
    If you send a command that doesn't return data, then you should either get a Message OK "FB" or an NG Code "FA".
       At this point, I don't think that it's necessary to reverify the actual entered value.

    If you send a command that does have return data, you need to WAIT for the data to be deposited in the buffer from the CIV Comms.
       HC-05 BT Version 2.0): This may take up to 5 seconds or sometimes more if using HC.  I often have to resend the request.  (Bluetooth comms seems to be very slow!)
       ESP-WROOM-32: (BT Version 5.0) Works much better, return is in < 50 milliseconds.  Works very well for "Transceive" mode.
         The default settings on the ESP32 seem to be just fine.  I still haven't successfully gotten into the "AT" commands in the ESP32.  Name is in the setup() declaration.

  If you try an HC-05, the BLUETOOTH module needs to be at 57600,0,0.  (HC-05 really doesn't work well with the IC-705 (BT Version 2.0)
    Need to use the AT commands to program it, but NEED to use an Arduino MEGA.  NOT a NANO with SofwareSerial.h (not fast enough).
    This is a bit tricky, but if you have MEGA setup() with Serial3.begin(57600); and it works, then you have it working.
      Once you change the Baud in the AT commands, you need to change the speed declaration in the setup() for that comm port.
    Often, when programming using the "at+uart=57600,0,0," it may show "at+uart?" as "57600,,,," but that seems to work.  If you REBOOT the HC-05, then the at+uart? may show "4800,0,0".
        Don't do it, just take it after it returns the "57600,,,," and use it.  As long as the commands return a value with setup() setting Serial3.begin(57600), it's working.
*/

//NOTE that I've hard coded the IC-705 Radio Address
uint8_t  RADIO_ADDRESS = 0xA4;     //Transiever address, hard coded.
const byte BROADCAST_ADDRESS = 0x00; //Broadcast address
const byte CONTROLLER_ADDRESS = 0xE0; //Controller address

//Start & Stop bytes
const byte START_BYTE = 0xFE; //Start byte
const byte STOP_BYTE = 0xFD; //Stop byte

//Commands:
const byte CMD_READ_FREQ = 0x03; //Read operating frequency data
const byte CMD_TRANSCEIVE_FREQ = 0x00;
const byte CMD_RF_Power = 0x14; //Read operating RF Power value
const byte CMD_POWER_OFF = 0x18; //Command to turn Rig Power Off.
const byte CMD_MODE = 0x01;   //Mode (LSB,USB, CW, etc. & Filter setting.  Sent out during 'Transceive' mode Band Change.
const byte TransceiveCmd = 0x1A; //Read & Write the 'Transceive' mode state
const byte AckMsg = 0xFB; //Received an ACK from the ICOM rig.
const byte NacMsg = 0xFA; //Receive a "Not Good" message from the rig.
//Array for decoding the Frequency:
const uint32_t decMulti[] = {1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};


uint32_t processCatMessages(boolean WaitCmd) {
  //Set WaitCmd to 'true' if a SPECIFIC command was sent, and you are waiting for a response.
  //Set WaitCmd to 'false' if you are just checking the buffer (i.e.: Transceive Mode).
  //Returns the data Read from: Frequency,

  uint8_t read_buffer[12];   //Read buffer
  int Count = 0;
  static int Average;
  static int AverageCount;
  //static unsigned long Total;


  //A SPECIFIC command was sent, wait for the response!
  if (WaitCmd) {
    //unsigned long StartWait = millis();
    do {
      delay(10);
      Count++;
    } while ((SerialBT.available() == 0) && (Count < 20)); //Currently set at a 200ms timeout...
    
    //Serial.print(F(" processCatMessages Count = ")); Serial.print(Count); Serial.print(F(" Out of 200."));
    //Serial.print(F("      BT Wait Time = ")); Serial.println(millis() - StartWait);
    if (SerialBT.available() == 0) return 0;
  }

  while (SerialBT.available()) {
    if (readLine(read_buffer, sizeof(read_buffer)) > 0) {
      if ((read_buffer[0] == START_BYTE) && (read_buffer[1] == START_BYTE)) {
        if ((read_buffer[3] == RADIO_ADDRESS) || (read_buffer[4] == RADIO_ADDRESS)) {
          if ((read_buffer[2] == BROADCAST_ADDRESS) || (read_buffer[2] == CONTROLLER_ADDRESS)) {
            switch (read_buffer[4]) {
              case CMD_TRANSCEIVE_FREQ: { //0x00
                  //Serial.println("Transceive");
                  return printFrequency(read_buffer);
                  break;
                }
              case CMD_READ_FREQ: { //0x03
                  //Serial.println("Read Frequency");
                  return printFrequency(read_buffer);
                  break;
                }
              case CMD_MODE: { //0x01
                  //Serial.println("Mode Update (Don't Care)");
                  return 0;
                  break;
                }
              case TransceiveCmd: { //Read Transceive State
                  //Data is in read_buffer[8].
                  return read_buffer[8];
                }
              case CMD_RF_Power: {  //0x14
                  //Decode the data from buffer 6 & 7 using bcd2dec().
                  uint8_t MSB = bcd2dec(read_buffer[6]);
                  uint8_t LSB = bcd2dec(read_buffer[7]);
                  return (MSB * 100) + LSB;
                  break;
                }
              case AckMsg: { //Ack Message (FB) from ICOM
                  //Serial.println(F("  FB Recieved, Returned 255."));
                  return 255;
                }
              case NacMsg: { //Ack Message (FA) from ICOM
                  //Serial.println(F("  FB Recieved, Returned 254."));
                  return 254;
                }
              default: {
                  //Any other messages that may come through, print them out...
                  //Serial.print("           ##  ERROR?       Default <");
                  //                  for (uint8_t i = 0; i < sizeof(read_buffer); i++) {
                  //                    if (read_buffer[i] < 16)Serial.print("0");
                  //                    Serial.print(read_buffer[i], HEX);
                  //                    Serial.print(" ");
                  //                    if (read_buffer[i] == STOP_BYTE) break;
                  //                  }
                  //                  Serial.println(">");
                  break;
                }
            }
          }
          else {
            //Serial.println(F(" else Broadcast/Controller Address"));
          }
        }
        else {
          //Serial.println(F(" else buffer[3,4] Radio Address"));
        }
      }
      else {
        //Serial.println(F(" else StartByte Else"));
      }
    }
    else {
      //Serial.println(F(" else Buffer ==0"));
    }
  }
  return 0;
}



uint8_t readLine(uint8_t buffer[], uint8_t ArraySize) {
  //This function reads the line from the CIV Buffer and returns it in the 'buffer[]' array.
  //  NOTE: I don't think the Timeout is required at all.
  uint8_t NextByte;
  uint8_t counter = 0;

  while (SerialBT.available()) {
    NextByte = SerialBT.read(); // Read 1 byte at a time.
    //delay(5);
    buffer[counter++] = NextByte;
    if (STOP_BYTE == NextByte) break;
    if (counter >= ArraySize) return 0;
  }
  //Temporarily, print out ALL lines that are received.
  //  Serial.print("        ## readLine Data<");
  //  for (uint8_t i = 0; i < counter; i++) {
  //    if (buffer[i] < 16)Serial.print("0");
  //    Serial.print(buffer[i], HEX);
  //    Serial.print(" ");
  //    if (buffer[i] == STOP_BYTE)break;
  //  }
  //  Serial.println(">");

  return counter;
}



uint32_t printFrequency(uint8_t buffer[]) {
  //Freq<FE FE E0 A4 03 00 00 00 01 00 FD>  1.0 MHz
  //Freq<FE FE E0 A4 03 00 00 00 33 04 FD>  433.0 MHz
  //Start at the Right most digit, read_buffer[9]  (MSD):
  uint32_t frequency = 0;
  for (uint8_t i = 0; i < 5; i++) {
    if (buffer[9 - i] == 0xFD) continue; //End of Message, break out of loop...
    frequency += (buffer[9 - i] >> 4) * decMulti[i * 2];
    frequency += (buffer[9 - i] & 0x0F) * decMulti[i * 2 + 1];
  }
  //We divide by 1000 to give freq in KHz. Precision is not requried.
  return (frequency / 1000);
}

uint8_t bcd2dec(uint8_t n) {
  //Used to Read RF Power
  return n - 6 * (n / 16);
}

void CivSendCmd(uint8_t buffer[], uint8_t ArraySize) {
  //This function is used to SEND the commands (from the 'buffer[]' to the CIV port

  //Serial.print(F("  SEND CMD: <"));
  for (uint8_t i = 0; i < ArraySize; i++) {
    SerialBT.write(buffer[i]);
    //Serial.print(buffer[i], HEX); Serial.print(" ");
  }
  //Serial.println(F("> SEND Command Complete"));
}


boolean WaitOkMsg(void) {
  //Returns true if it fails
  //If a command was send that does NOT expect specific data return, then we wait for the OK Message (FB) return data.
  //  If the command failed, then the Message NG "FA" will be returned.
  uint8_t Return;

  Return = processCatMessages(true);

  if (Return == 255) {
    //Serial.println(F("    GOT RETURN OF 255 After Command"));
    return false;
  }
  else if (Return == 254) {
    //Serial.println(F("      ERROR, NG CODE OF 254 (NacMsg) After Command"));
    return true;
  }
  else {
    //Serial.println(F("      ERROR, NO RETURN of OK Message After Command"));
    return true;
  }
}




//Commands:

uint32_t CIV_Read_Frequency(void) {
  // Read the current Frequency and return Frequency as 3760, 28500, etc...
  //Don't Read the message, it will be picked up by the normal processCatMessages() loop...
  //  No Sub Cmd's required:
  //static int ErrorCount;

  byte ReadFreq[] = {START_BYTE, START_BYTE, RADIO_ADDRESS, CONTROLLER_ADDRESS, CMD_READ_FREQ, STOP_BYTE};
  uint32_t ReturnFreq = 0;
  byte Count = 0;

  CivSendCmd(ReadFreq, sizeof(ReadFreq));
  //Count == 0 ? delay(0) :
  //  delay(10);
  ReturnFreq = processCatMessages(true);
  if (ReturnFreq < 256) {
    //ErrorCount++;
    //Serial.print(F("ERR"));
  }
  //Serial.print(F("      CIV_Read_Frequency read: ")); Serial.print(ReturnFreq); Serial.print(F("  ErrorCount:")); Serial.println(ErrorCount);
  return   ReturnFreq;
}


void CIV_Power_Off() {
  //For the Power OFF command, need to add the Sub Cmd: 0x00.  No Expected Return (it should shut off!)
  byte PowerOff[] = {START_BYTE, START_BYTE, RADIO_ADDRESS, CONTROLLER_ADDRESS, CMD_POWER_OFF, 0x00, STOP_BYTE};
  CivSendCmd(PowerOff, sizeof(PowerOff));
  delay(200);
  //Resend the command, just to be on the safe side...
  CivSendCmd(PowerOff, sizeof(PowerOff));
}


boolean CIV_Transceive_On_Off(boolean On_Off) {
  //Set Transceive mode on/off: Cmd: 0x1A, Sub Cmd 05, 0131, 00/01 for off or on.
  //  Not longer READ the value, just turn off and verify the "FB" return.
  //   returns true if it fails
  boolean Return;
  byte CMD;
  byte Count = 0;
  if (On_Off == 1) CMD = 0x01;
  else CMD = 0x00;

  uint8_t WriteTransceive[] = {START_BYTE, START_BYTE, RADIO_ADDRESS, CONTROLLER_ADDRESS, 0x1A, 0x05, 0x01, 0x31, CMD, STOP_BYTE};

  CivSendCmd(WriteTransceive, sizeof(WriteTransceive));
  Return = WaitOkMsg();

  //Serial.print(F("      Read Transceive State returned: ")); Serial.println(Return);
  return Return;
}


boolean CIV_ReadTransceiveState() {
  //Send the command to READ the Transceive function:
  uint8_t ReadTransceive[] = {START_BYTE, START_BYTE, RADIO_ADDRESS, CONTROLLER_ADDRESS, 0x1A, 0x05, 0x01, 0x31, STOP_BYTE};

  CivSendCmd(ReadTransceive, sizeof(ReadTransceive));
  //Read the return...
  uint8_t Return = processCatMessages(true);
  return Return;
}


boolean CIV_SetPower(uint32_t Watts) {
  // Returns true if it fails
  //Works OK now, set watts, 0, 1, 2...9, 10.
  // Rig Requires Hex Values, but never using A,B,C,D,E or F!
  uint8_t MSB = 0;
  uint8_t LSB = 0;
  byte Count = 0;

  //Set power only in Whole Watts, 0-10.
  switch (Watts) {
    case 0: {
        MSB = 0x00;  //Values in HEX
        LSB = 0x00;
        break;
      }
    case 1: {
        MSB = 0x00;
        LSB = 0x26;
        break;
      }
    case 2: {
        MSB = 0x00;
        LSB = 0x51;
        break;
      }
    case 3: {
        MSB = 0x00;
        LSB = 0x77;
        break;
      }
    case 4: {
        MSB = 0x01;
        LSB = 0x02;
        break;
      }
    case 5: {
        MSB = 0x01;
        LSB = 0x28;
        break;
      }
    case 6: {
        MSB = 0x01;
        LSB = 0x53;
        break;
      }
    case 7: {
        MSB = 0x01;
        LSB = 0x79;
        break;
      }
    case 8: {
        MSB = 0x02;
        LSB = 0x04;
        break;
      }
    case 9: {
        MSB = 0x02;
        LSB = 0x30;
        break;
      }
    case 10: {
        MSB = 0x02;
        LSB = 0x55;
        break;
      }
  }

  //For the RF Power, need to add the Sub Cmd: 0x0A
  uint8_t RfPower[] = {START_BYTE, START_BYTE, RADIO_ADDRESS, CONTROLLER_ADDRESS, CMD_RF_Power, 0x0A, MSB, LSB, STOP_BYTE};

  //Serial.print(F("  Setting Power to: ")); Serial.print(MSB, HEX); Serial.print(" "); Serial.println(LSB, HEX);
  CivSendCmd(RfPower, sizeof(RfPower));
  return WaitOkMsg();
}

uint32_t CIV_Read_RF_Power() {
  uint32_t Value = 0;
  byte Count = 0;
  //Serial.print(F(" Read."));
  uint8_t RfPower[] = {START_BYTE, START_BYTE, RADIO_ADDRESS, CONTROLLER_ADDRESS, CMD_RF_Power, 0x0A, STOP_BYTE};

  do {
    //Serial.println(F("  Sending Read Power Command."));
    CivSendCmd(RfPower, sizeof(RfPower));
    //delay(100);
    Value = processCatMessages(true);
    if (Value == 255) Value = processCatMessages(true);
    Count++;
  } while ((Value == 0) && (Count <= 3));

  //Serial.println();
  uint32_t Watts = map(Value, 0, 255, 0, 10);
  return Watts;
}
