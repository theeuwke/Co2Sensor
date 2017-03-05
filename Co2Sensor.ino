#include <Homie.h>
#include <SoftwareSerial.h>

SoftwareSerial *SwSerial;

#if 0 //old
// 9-bytes CMD PPM read command
byte mhzCmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79};
byte mhzResp[9]; // 9 bytes bytes response

float getMhzValue(void)
{
  int i;
  unsigned int ppm = 0;
  byte crc = 0;

  SwSerial->write(mhzCmd, 9);
  memset(mhzResp, 0, 9);
  SwSerial->readBytes(mhzResp, 9);

  for (i = 1; i < 8; i++) 
    crc+=mhzResp[i];
  crc = 255 - crc;
  crc++;

  if ( !(mhzResp[0] == 0xFF && mhzResp[1] == 0x86 && mhzResp[8] == crc) ) {
    String log = F("MHZ19: CRC error: ");
    log += String(crc); log += " / "; log += String(mhzResp[8]);
    Homie.getLogger() << "error: " << log << endl;
    return (float) -1;
  } else {
    unsigned int mhzRespHigh = (unsigned int) mhzResp[2];
    unsigned int mhzRespLow = (unsigned int) mhzResp[3];
    ppm = (256 * mhzRespHigh) + mhzRespLow;
  }
  
  Homie.getLogger() << "MHZ19: PPM value: " << ppm << endl;
  
  return (float) ppm;
}
#endif

#if 0 //old too
int getMhzValue()
{

  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  // command to ask for data
  char response[9]; // for answer

  SwSerial->write(cmd, 9); //request PPM CO2
  SwSerial->readBytes(response, 9);
  if (response[0] != 0xFF)
  {
    Homie.getLogger() << "Wrong starting byte from co2 sensor!" << endl;
    return -1;
  }
  if (response[1] != 0x86)
  {
    Homie.getLogger() << "Wrong command from co2 sensor!" << endl;
    return -1;
  }

  int responseHigh = (int) response[2];
  int responseLow = (int) response[3];
  int ppm = (256 * responseHigh) + responseLow;
  return ppm;
}
#endif

#include "mhz19.h"

static bool exchange_command(uint8_t cmd, uint8_t data[], int timeout)
{
    // create command buffer
    uint8_t buf[9];
    int len = prepare_tx(cmd, data, buf, sizeof(buf));

    // send the command
    SwSerial->write(buf, len);

    // wait for response
    long start = millis();
    while ((millis() - start) < timeout) {
        if (SwSerial->available() > 0) {
            uint8_t b = SwSerial->read();
            if (process_rx(b, cmd, data)) {
                return true;
            }
        }
    }

    return false;
}

static bool read_temp_co2(int *co2, int *temp)
{
    uint8_t data[] = {0, 0, 0, 0, 0, 0};
    bool result = exchange_command(0x86, data, 3000);
    if (result) {
        *co2 = (data[0] << 8) + data[1];
        *temp = data[2] - 40;
#if 1
        char raw[32];
        sprintf(raw, "RAW: %02X %02X %02X %02X %02X %02X", data[0], data[1], data[2], data[3], data[4], data[5]);
        //SwSerial->println(raw);
        Homie.getLogger() << raw << endl;
#endif
    }
    return result;
}

/* homie */
const int CO2_INTERVAL = 30;
unsigned long lastCo2ValueSent = 0;

HomieSetting<long> rxSetting("RxPin", "RX Pin connected to the MH-Z19"); // id, description
HomieSetting<long> txSetting("TxPin", "TX Pin connected to the MH-Z19"); // id, description

HomieNode Co2Node("co2", "co2");
HomieNode TempNode("temp", "temp");

void setupHandler() {
  Co2Node.setProperty("unit").send("ppm");
  TempNode.setProperty("unit").send("c");
}

void loopHandler() {
  int co2, temp;

  if ((millis() - lastCo2ValueSent) >= (CO2_INTERVAL * 1000UL)) {
    //float value = getMhzValue();
    if (read_temp_co2(&co2, &temp)) {
      Homie.getLogger() << "Co2 value: " << co2 << " PPM" << endl;
      Homie.getLogger() << "Temp value: " << temp << " C" << endl;
      Co2Node.setProperty("ppm").send(String(co2));
      Co2Node.setProperty("temp").send(String(temp));
      lastCo2ValueSent = millis();
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial << endl << endl;
  Homie_setFirmware("co2-sensor", "1.0.0");
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  Co2Node.advertise("unit");
  Co2Node.advertise("ppm");

  TempNode.advertise("unit");
  TempNode.advertise("c");

  rxSetting.setDefaultValue(D7).setValidator([] (long candidate) {
    return (candidate >= D0) && (candidate <= D8);
  });

  txSetting.setDefaultValue(D8).setValidator([] (long candidate) {
    return (candidate >= D0) && (candidate <= D8);
  });

  lastCo2ValueSent = millis();

  SwSerial = new SoftwareSerial(rxSetting.get(), txSetting.get());
  SwSerial->begin(9600);
  
  Homie.setup();
}

void loop() {
  Homie.loop();
}


