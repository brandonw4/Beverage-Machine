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
#include "MachineExceptions.hpp"
#include "SecretEnv.h" //holds CERT,

#define CS_PIN 5

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN = 33;
const int STABILIZING_TIME = 3000;    // tare preciscion can be improved by adding a few seconds of stabilizing time
const float CALIBRATION_FACTOR = 696; // this calibration factor is adjusted according to my load cell
const uint8_t LOADCELL_GAIN = 128;

const size_t MOTOR_COUNT = 8;
const size_t BEV_COUNT = 12;

struct Bottle
{
    String name;
    int id = -1;
    bool active = false;
    bool isShot = false;
    double costPerOz = 0.0;
    double estimatedCapacity = 0.0;
    double totalCapacity = 0.0;
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
    -Yellow:
    -Red: 63488
    -Green: 1024
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

class Beverage
{
private:
public:
    String name;
    int id = -1;
    bool isActive = true;
    double ozArr[MOTOR_COUNT];
    String additionalInstructions = "";
};

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

    // SD CARD
    File config;
    File data;
    File log;
    // File paymentLog

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

    // MQTT server and WiFi
    void initWifi();
    void initMqtt();
    // TODO: WiFi reconnect
    // TODO: TX Functions (bottles, general status etc.)
    const int MQTT_PORT = 8883;
    const int WIFI_TIMEOUT_MS = 10000; // 10 seconds
    const int MQTT_TIMEOUT_MS = 20000; // 20 seconds
    const int MQTT_KEEP_ALIVE = 20;    // 5 seconds
    void txMachineStatus();

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
    double dispense(int motorId, double oz);

    // Touchscreen other functions
    std::vector<InputData> formatInputString(String inputStr); // formats input string into vector of InputData
    std::vector<InputData> checkForInput();                    // checks serial for input, returns vector of InputData
    void inputDecisionTree(std::vector<InputData> &inputs);

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