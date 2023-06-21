#ifndef Machine_hpp
#define Machine_hpp

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
//#include <HX711.h>
#include <HX711_ADC.h>
#include <stdexcept>
#include <vector>
#include <sstream>

#define CS_PIN 5

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 32;
const int LOADCELL_SCK_PIN = 33;
const int STABILIZING_TIME = 400; // tare preciscion can be improved by adding a few seconds of stabilizing time
const float CALIBRATION_FACTOR = 696; // this calibration factor is adjusted according to my load cell
const uint8_t LOADCELL_GAIN = 128;

const size_t MOTOR_COUNT = 8;
const size_t BEV_COUNT = 12;

class CupNotFoundException : public std::exception
{
public:
    CupNotFoundException(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

private:
    std::string message_;
};

class BeverageDisabledException : public std::exception
{
public:
    BeverageDisabledException(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

private:
    std::string message_;
};

class BottleDisabledException : public std::exception
{
public:
    BottleDisabledException(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

private:
    std::string message_;
};

class MotorTimeoutException : public std::exception
{
public:
    MotorTimeoutException(const std::string &message) : message_(message) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

private:
    std::string message_;
};

class CupRemovedException : public std::exception
{
public:
    CupRemovedException(const std::string &message, double oz)
    : message_(message), dispensed_oz_(oz) {}

    const char *what() const noexcept override
    {
        return message_.c_str();
    }

    double get_dispensed_oz() const noexcept
    {
        return dispensed_oz_;
    }

private:
    std::string message_;
    double dispensed_oz_;
};


struct Bottle
{
    String name;
    int id = -1;
    bool active = false;
    bool isShot = false;
    double costPerOz = 0.0;
    double estimatedCapacity = 0.0;
};

struct InputData {
    String cmd;
    int id;
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
    */
   /*
   Colors:
   -White: 65535
   -Black: 0
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
    //String checkForInput();
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
    HX711_ADC LoadCell;
    int tareWeight = 0;
    // FUTURE: var. to track what the weight scale usually is in prod. if its much higher, there's prob. smth. on there.
public:
    LoadScale() : LoadCell(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN) {
        // Constructor to initialize the LoadCell object with the provided pins
    }
    void initLoadCell();
    void tareScale();
    int getCurrentWeight();

};

class Machine
{
private:
    //const size_t PASSCODE[] = {1, 0, 2, 5}; // ik im not proud of this  
    const int CALIBRATION_WAIT_MS = 4500; // Time to give user to see message, then set a calibration value.
    const int MIN_CUP_WEIGHT = 5; // Minimum weight of cup to be considered a cup.
    
    //const double SCALE_OZ_FACTOR = 20.525 - HOSE_LAG_VALUE;
    const double SCALE_OZ_FACTOR = 20.525;
    const int MOTOR_TIMEOUT_MS = 10000; // 10 seconds
    const double CUP_REMOVAL_THRESHOLD = 2;

    // SD CARD
    File config;
    File data;
    File log;
    // File paymentLog

    std::vector<Bottle> bottles;
    std::vector<Beverage> beverages;
    bool authCocktail = false;  // require auth on cocktail
    bool authShots = false;     // require auth on shots
    bool priceMode = true;      // TODO: Add to config
    bool requirePayment = true; // TODO: Add to config (priceMode must be true also)
    bool debugMainPageOutput = true; //Display debug info on main page

    // load cell
    LoadScale loadCell;
    void calibrate();

    // filedata
    void initSD();
    void initConfig();
    void initBottles();
    void initData();

    void loadMainMenu();
    void loadAdminMenu();
    void loadCocktailMenu();

    double convertToScaleUnit(double oz);
    double convertToOz(double scaleUnit);
    void createBeverage(int id);
    double dispense(int motorId, double oz);

    //Touchscreen other functions
    InputData checkForInput();

public:
    TouchControl touchscreen;
    void boot();
    void makeSelection();
    void inputDecisionTree();
};

#endif /*Machine_hpp*/