
// Set a value for the different Bands, NOTE that this is ALSO the Eeprom read/write value for each band to set the Power Value.
const int i160m = 0;
const int i80m = 2;
const int i40m = 4;
const int i20m = 6;
const int i17m = 8;
const int i15m = 10;
const int i12m = 12;
const int i10m = 14;
const int i6m = 16;
const int i60m = 30;
const int i30m = 32;

byte FreqToBand(unsigned int TargetFreq, boolean &HamBand) {
  //Returns Band (as a byte variable) and HamBand so that we can disable the Amp when outside the HamBands (using the Bypass() function.
  //Convert the Frequency to Band

  //For efficiency, run through the Ham Bands First:
  if ((TargetFreq >= 1800) && (TargetFreq < 2000)) {  //Valid 160m
    HamBand = true;
    return  i160m;
  }
  else if ((TargetFreq >= 3500) && (TargetFreq < 4000)) { //Valid 80m
    HamBand = true;
    return  i80m;
  }
  //Currently, 60M antenna is NOT connected, the case below will bypass the 60M enable below, Amp not allowed anyway!
  else if ((TargetFreq >= 5300) && (TargetFreq < 5500)) { //Valid 60m
    HamBand = false;  //Disable Amp on 60M
    return  i60m;
  }
  else if ((TargetFreq >= 7000) && (TargetFreq < 7300))  { //Valid 40m
    HamBand = true;
    return  i40m;
  }
  else if ((TargetFreq >= 10100) && (TargetFreq < 10150)) { //Valid 30m
    HamBand = false;  //Amp NOT allowed on 30m
    return  i30m;  //30m enables the 40m band anyway...
  }
  else if ((TargetFreq >= 14000) && (TargetFreq < 14350)) { //Valid 20m
    HamBand = true;
    return  i20m;
  }
  else if ((TargetFreq >= 18068) && (TargetFreq < 18168)) { //Valid 17m
    HamBand = true;
    return  i17m;
  }
  else if ((TargetFreq >= 21000) && (TargetFreq < 21450)) { //Valid 15m
    HamBand = true;
    return  i15m;
  }
  else if ((TargetFreq >= 24890) && (TargetFreq < 24990)) { //Valid 12m
    HamBand = true;
    return  i12m;
  }
  else if ((TargetFreq >= 28000) && (TargetFreq < 29700)) { //Valid 10m
    HamBand = true;
    return  i10m;
  }
  else if ((TargetFreq >= 50100) && (TargetFreq < 54000)) { //Valid 6m
    HamBand = true;
    return  i6m;
  }

  //Out of Band Frequenies:
  else if (TargetFreq < 1800) {
    HamBand = false;
    return i160m;
  }
  else if ((TargetFreq >= 2000) && (TargetFreq < 3000)) {
    HamBand = false;
    return  i160m;
  }
  else if ((TargetFreq >= 3000) && (TargetFreq < 3500)) {
    HamBand = false;
    return  i80m;
  }
  else if ((TargetFreq >= 4000) && (TargetFreq < 6000)) {
    HamBand = false;
    return  i80m;
  }
  else if ((TargetFreq >= 6000) && (TargetFreq < 7000)) {
    HamBand = false;
    return  i40m;
  }
  else if ((TargetFreq >= 7300) && (TargetFreq < 10100)) {
    HamBand = false;
    return  i40m;
  }
  else if ((TargetFreq >= 10150) && (TargetFreq < 12000)) {
    HamBand = false;
    return  i40m;
  }
  else if ((TargetFreq >= 12000) && (TargetFreq < 14000)) {
    HamBand = false;
    return  i20m;
  }
  else if ((TargetFreq >= 14350) && (TargetFreq < 18068)) {
    HamBand = false;
    return  i20m;
  }
  else if ((TargetFreq >= 18168) && (TargetFreq < 21000)) {
    HamBand = false;
    return  i17m;
  }
  else if ((TargetFreq >= 12450) && (TargetFreq < 24890)) {
    HamBand = false;
    return  i15m;
  }
  else if ((TargetFreq >= 24990) && (TargetFreq < 28000)) {
    HamBand = false;
    return  i12m;
  }
  else if ((TargetFreq >= 29700) && (TargetFreq < 50100)) {
    HamBand = false;
    return  i10m;
  }
  else if (TargetFreq >= 54000)  {
    HamBand = false;
    return  i6m;
  }
  else return 255;
}



//Not really needed for the ESP32, more for testing.
String BandString(int CurrentBand) {
  //Return the String Name of the Current Band
  switch (CurrentBand) {
    //Always 5 chars
    case i160m: return "160m "; break;
    case i80m:  return "80m  "; break;
    case i60m:  return "60m  "; break;
    case i40m:  return "40m  "; break;
    case i30m:  return "30m  "; break;
    case i20m:  return "20m  "; break;
    case i17m:  return "17m  "; break;
    case i15m:  return "15m  "; break;
    case i12m:  return "12m  "; break;
    case i10m:  return "10m  "; break;
    case i6m:   return "6m   "; break;
    default:    return "Unkn "; break;
  }
}

