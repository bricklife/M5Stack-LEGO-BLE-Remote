#include <M5Stack.h>
#include <BLEDevice.h>

static BLEUUID serviceUUID("00001623-1212-EFDE-1623-785FEABCD123");
static BLEUUID characteristicUUID("00001624-1212-EFDE-1623-785FEABCD123");

static BLEAdvertisedDevice* connectingDevice = nullptr;
static BLERemoteCharacteristic* characteristic = nullptr;

static int8_t power = 0;

static void logData(char* prefix, uint8_t* data, size_t length) {
  Serial.print(prefix);
  for (uint8_t i = 0; i < length; i++) {
    Serial.printf("%02x ", data[i]);
  }
  Serial.println("");
}

static void clearUI() {
  M5.Lcd.fillRect(0, 120, 320, 120, TFT_BLACK);
}

static void drawPower(int8_t power) {
  clearUI();
  
  char buf[10];
  sprintf(buf, "%d", power);
  M5.Lcd.drawCentreString(buf, 160, 120, 4);
}

static void notifyCallback(BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool isNotify) {
  logData("<- ", data, length);
}

class MyClientCallback : public BLEClientCallbacks {

    void onConnect(BLEClient* client) {
      Serial.println("onConnect");
    }

    void onDisconnect(BLEClient* client) {
      Serial.println("onDisconnect");
      connectingDevice = nullptr;
      characteristic = nullptr;
      clearUI();
    }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

    void onResult(BLEAdvertisedDevice device) {
      Serial.print("Found device: ");
      Serial.println(device.toString().c_str());

      if (device.haveServiceUUID() && device.isAdvertisingService(serviceUUID) && device.haveManufacturerData()) {
        Serial.println("Found a LEGO BLE device!!");
        BLEDevice::getScan()->stop();

        connectingDevice = new BLEAdvertisedDevice(device);
      }
    }
};

static void startScan() {
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  scan->start(10);
}

static void stopScan() {
  BLEDevice::getScan()->stop();
}

static bool connect() {
  BLEClient* client = BLEDevice::createClient();
  client->setClientCallbacks(new MyClientCallback());
  client->connect(connectingDevice);

  BLERemoteService* service = client->getService(serviceUUID);
  if (service == nullptr) {
    Serial.println("Not found the service");
    client->disconnect();
    return false;
  }

  characteristic = service->getCharacteristic(characteristicUUID);
  if (characteristic == nullptr) {
    Serial.println("Not found the characteristic");
    client->disconnect();
    return false;
  }

  if (characteristic->canNotify()) {
    characteristic->registerForNotify(notifyCallback);
  }

  return true;
}

static void writeValue(uint8_t* data, size_t length) {
  characteristic->writeValue(data, length);

  logData("-> ", data, length);
}

static void sendSwitchOffCommand() {
  uint8_t data[] = {0x04, 0x00, 0x02, 0x01};

  writeValue(data, sizeof(data));
}

static void sendMotorPowerCommand(uint8_t port, int8_t power) {
  uint8_t data[8] = {0};
  data[0] = 0x08;
  data[1] = 0x00;
  data[2] = 0x81;
  data[3] = port;
  data[4] = 0x11;
  data[5] = 0x51;
  data[6] = 0x00;
  data[7] = power;

  writeValue(data, sizeof(data));
}

static void setPowerToAllMotors(int8_t power) {
  for (uint8_t port = 0; port < 4; port++) {
    sendMotorPowerCommand(port, power);
  }
  drawPower(power);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");

  M5.begin();
  M5.Power.begin();

  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextFont(2);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Lcd.println("Powered Up Remote");

  BLEDevice::init("");
  startScan();
}

void loop() {
  M5.update();

  if (connectingDevice != nullptr) {
    if (characteristic == nullptr) {
      if (connect()) {
        power = 0;
        drawPower(power);
      } else {
        Serial.println("Failed to make a connection...");
        connectingDevice = nullptr;
      }
    } else {
      if (M5.BtnA.wasReleased()) {
        if (power > -100) {
          power -= 10;
          setPowerToAllMotors(power);
        }
      } else if (M5.BtnB.wasReleased()) {
        power = 0;
        setPowerToAllMotors(power);
      } else if (M5.BtnC.wasReleased()) {
        if (power < 100) {
          power += 10;
          setPowerToAllMotors(power);
        }
      } else if (M5.BtnB.wasReleasefor(1000)) {
        sendSwitchOffCommand();
      }
    }
  }
}
