#ifndef Machine_hpp
#define Machine_hpp

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <HX711_ADC.h>
#include <vector>
#include <sstream>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "MachineExceptions.hpp"
#include "SecretEnv.h" //holds CERT,
#include <FastLED.h>
#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>

#define CS_PIN 5

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN = 33;
const int STABILIZING_TIME = 3000;    // tare preciscion can be improved by adding a few seconds of stabilizing time
const float CALIBRATION_FACTOR = 696; // this calibration factor is adjusted according to my load cell
const uint8_t LOADCELL_GAIN = 128;

const size_t MOTOR_COUNT = 8;
const size_t BEV_COUNT = 12;

class MotorControl
{
    // class to take PCA9685 and write to it to control motors
public:
    // TODO: Fix the construction of this!!!
    Adafruit_PWMServoDriver pwm;
    void init()
    {
        pwm = Adafruit_PWMServoDriver();
        pwm.begin();
        Serial.println("PCA9685 Pump Control Init");
    }
    void runMotor(int id, bool run)
    {
        if (run)
        {
            pwm.setPWM(id, 0, 4095);
            Serial.println("Running motor " + String(id));
        }
        else
        {
            pwm.setPWM(id, 0, 0);
            Serial.println("Stopping motor " + String(id));
        }
    }
};

class FrontLEDControl
{
private:
    // LED Wiring
    static const int FRONT_LED_PIN = 27;
    static const int FRONT_LED_COUNT = 40;
    CRGB leds[FRONT_LED_COUNT];
    int lastUpdatedLed = -1;

public:
    void init()
    {
        FastLED.addLeds<NEOPIXEL, FRONT_LED_PIN>(leds, FRONT_LED_COUNT);
        FastLED.setBrightness(255);
        standby();
    }
    void clear();
    void standby();
    void updateProgress(double percent);
    void resetProgress() { lastUpdatedLed = -1; }
    void successAnimation();
    void errorAnimation();
};

struct Bottle
{
    String name;
    int id = -1;
    bool active = false;
    bool isShot = false;
    double costPerOz = 0.0;
    double estimatedCapacity = 0.0;
    double totalCapacity = 0.0;

    // convert Bottle to JSON
    String toJson() const
    {
        StaticJsonDocument<200> doc;
        doc["name"] = name;
        doc["_id"] = id;
        doc["active"] = active;
        doc["isShot"] = isShot;
        doc["costPerOz"] = costPerOz;
        doc["estimatedCapacity"] = estimatedCapacity;
        doc["totalCapacity"] = totalCapacity;
        String output;
        serializeJson(doc, output);
        return output;
    }

    // Method to initialize Bottle from JSON
    static Bottle fromJson(const String &json)
    {
        StaticJsonDocument<200> doc;
        deserializeJson(doc, json);
        Bottle bottle;
        bottle.name = doc["name"].as<String>();
        bottle.id = doc["_id"];
        bottle.active = doc["active"];
        bottle.isShot = doc["isShot"];
        bottle.costPerOz = doc["costPerOz"];
        bottle.estimatedCapacity = doc["estimatedCapacity"];
        bottle.totalCapacity = doc["totalCapacity"];
        return bottle;
    }
};

struct Beverage
{
    String name;
    int id = -1;
    bool isActive = true;
    double ozArr[MOTOR_COUNT];
    String additionalInstructions = "";

    // convert Beverage to JSON
    String toJson() const
    {
        StaticJsonDocument<300> doc;
        doc["name"] = name;
        doc["_id"] = id;
        doc["isActive"] = isActive;
        JsonArray ozArrJson = doc.createNestedArray("ozArr");
        for (double oz : ozArr)
        {
            ozArrJson.add(oz);
        }
        doc["additionalInstructions"] = additionalInstructions;
        String output;
        serializeJson(doc, output);
        return output;
    }

    // static method to initalize a beverage from JSON
    static Beverage fromJson(const String &json)
    {
        StaticJsonDocument<300> doc;
        deserializeJson(doc, json);
        Beverage bev;
        bev.name = doc["name"].as<String>();
        bev.id = doc["_id"];
        bev.isActive = doc["isActive"];
        for (int i = 0; i < doc["ozArr"].size(); i++)
        {
            bev.ozArr[i] = doc["ozArr"][i];
        }
        bev.additionalInstructions = doc["additionalInstructions"].as<String>();
        return bev;
    }
};

struct InputData
{
    String cmd;
    String value;
}; // struct InputData

class TouchControl
{
    /*
    Page Mappings:
    0: start
    1: home
    2:
    3: auth
    4: admin
    5: datalog
    6: keybdB (locked)
    7: ctail1
    8: dispense
    9: motorControl
    10: bevControl
    11: webControl
    12 editMotorCap
    */
    /*
    Colors:
    -White: 65535
    -Black: 0
    -Yellow: 65088
    -Red: 63488
    -Green: 1024
    -Grey: 33808
    -Navy: 297
    */
private:
    void touchOutput(String str); // touch screen requires specific format
    void flushStream()
    {
        Serial2.write(0xff);
        Serial2.write(0xff);
        Serial2.write(0xff);
    };

public:
    // String checkForInput();
    int currentPage = 0;

    void printDebug(String str)
    {
        Serial.print("SENT T.SCREEN: ");
        Serial.println(str);
        touchOutput(str);
    }; // will print to both console serial and to lcd serial
    void print(String str)
    {
        touchOutput(str);
    }; // void print()
    /* void controlCurPage:
    Follow Nextion guidelines.
    */
    void controlCurPage(String item, String cmd, String val);
    void changePage(String pageNum)
    {
        flushStream();
        String send = "page ";
        send.concat(pageNum);
        touchOutput(send);
        currentPage = pageNum.toInt();
        Serial.println("Changed to page: " + String(pageNum));
    }; // void changePage()

}; // class TouchControl

class LoadScale
{
private:
    int tareWeight = 0;
    // FUTURE: var. to track what the weight scale usually is in prod. if its much higher, there's prob. smth. on there.
public:
    LoadScale() : LoadCell(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN)
    {
        // Constructor to initialize the LoadCell object with the provided pins
    }
    HX711_ADC LoadCell;
    void initLoadCell();
    void tareScale();
    int getCurrentWeight();
};

class Machine
{
private:
    // const size_t PASSCODE[] = {1, 0, 2, 5}; // ik im not proud of this
    const int CALIBRATION_WAIT_MS = 4500;  // Time to give user to see message, then set a calibration value.
    const int MIN_CUP_WEIGHT = 5;          // Minimum weight of cup to be considered a cup.
    const int COUNTDOWN_MSG_MS = 4000;     // MUST BE FULL SECONDS (1000, 2000, etc)
    const int COUNTDOWN_BEV_MSG_MS = 8000; // MUST BE FULL SECONDS (1000, 2000, etc)

    // const double SCALE_OZ_FACTOR = 20.525 - HOSE_LAG_VALUE;
    const double SCALE_OZ_FACTOR = 20.525;
    const int MOTOR_TIMEOUT_MS = 12000; // 10 seconds
    const double CUP_REMOVAL_THRESHOLD = 2;

    bool isCalibrated = false;

    // SD CARD
    File config;
    File data;
    File log;
    // File paymentLog

    // NVS
    Preferences preferences;

    std::vector<Bottle> bottles;
    std::vector<Beverage> beverages;
    bool authCocktail = false;       // require auth on cocktail
    bool authShots = false;          // require auth on shots
    bool priceMode = true;           // TODO: Add to config
    bool requirePayment = true;      // TODO: Add to config (priceMode must be true also)
    bool debugMainPageOutput = true; // Display debug info on main page

    void calibrate();

    // filedata
    void initSD();
    void initConfig();
    void initBottles();
    void initData();
    // NVS data
    void initNVSBottles();
    void initNVSBev();
    void saveNVSBottle(int bottleId, const Bottle &bottle);
    void saveAllNVSBottles();
    void saveNVSBev(int bevId, const Beverage &bev);
    void saveAllNVSBevs();

    // MQTT server and WiFi
    void initWifi();
    void initMqtt();
    // TODO: WiFi reconnect
    // TODO: TX Functions (bottles, general status etc.)
    const int MQTT_PORT = 8883;
    const int WIFI_TIMEOUT_MS = 10000; // 10 seconds
    const int MQTT_TIMEOUT_MS = 20000; // 20 seconds
    const int MQTT_KEEP_ALIVE = 6;     // 6 seconds
    void txMachineStatus();
    void rxBottleStatus(const String &json);

    void loadMainMenu();
    void loadAdminMenu();
    void loadMotorControlMenu();
    void loadMotorEditMenu(int motorId);
    int currentMotorEditId = -1;
    void loadCocktailMenu();

    // will run countdown timer after val string. if displayCountdown is false, will just display text on page and then change
    // DESTINATION PAGE MUST BE AFTER THIS FUNCTION CALL, it does NOT change the page!
    void countdownMsg(int timeMillis, bool displayCountdown, String item, String destPageName);

    double convertToScaleUnit(double oz);
    double convertToOz(double scaleUnit);
    void createBeverage(int id);
    double dispense(int motorId, double oz, double runningOz, double totalOz); // runningOz and totalOz are used for accurate led progress bar

    // Touchscreen other functions
    std::vector<InputData> formatInputString(String inputStr); // formats input string into vector of InputData
    std::vector<InputData> checkForInput();                    // checks serial for input, returns vector of InputData
    void inputDecisionTree(std::vector<InputData> &inputs);

    // Front LED
    FrontLEDControl frontLED;

    // Pump Control
    MotorControl motorControl;

public:
    Machine(size_t bottleCount, size_t bevCount);

    TouchControl touchscreen;
    // load cell
    LoadScale loadCell;
    bool debugPrintWeightSerial = false; // in loop() print the weight to serial
    bool debugPrintWeightSerialDispense = true;
    // MQTT
    WiFiClientSecure wifiClient;
    PubSubClient mqttClient;
    void connectMqtt();

    void loopCheckSerial();
    void boot();

    void mqttCallback(char *topic, byte *payload, unsigned int length);
};

#endif /*Machine_hpp*/