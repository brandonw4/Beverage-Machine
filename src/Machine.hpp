#ifndef Machine_hpp
#define Machine_hpp

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <stdexcept>
#include <vector>
#include <sstream>

#define CS_PIN 5

const size_t MOTOR_COUNT = 8;
const size_t BEV_COUNT = 12;

const size_t PASSCODE[] = {1, 0, 2, 5}; // ik im not proud of this

struct Bottle
{
    String name;
    int id = -1;
    bool active = false;
    bool isShot = false;
    double costPerOz = 0.0;
    double estimatedCapacity = 0.0;
};

class TouchControl
{
private:
    void touchOutput(String str); // touch screen requires specific format
    void flushStream()
    {
        Serial2.write(0xff);
        Serial2.write(0xff);
        Serial2.write(0xff);
    };

public:
    /*
        TODO: create a function that abstracts the controlling of screen, like buttonId, etc.
    */
    String checkForInput();
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
    void createBeverage(std::vector<Bottle> &bottles);
};

class Machine
{
private:
    // SD CARD
    File config;
    File data;
    File log;
    // File paymentLog

    std::vector<Bottle> bottles;
    std::vector<Beverage> beverages;
    bool authCocktail = false; // require auth on cocktail
    bool authShots = false;    // require auth on shots

    // filedata
    void initSD();
    void initConfig();
    void initBottles();
    void initData();

    void loadMainMenu();
    void loadAdminMenu();
    void loadCocktailMenu();

public:
    TouchControl touchscreen;
    void boot();
    void makeSelection();
};

#endif /*Machine_hpp*/