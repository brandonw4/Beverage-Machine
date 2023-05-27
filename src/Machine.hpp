#ifndef Machine_hpp
#define Machine_hpp

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <stdexcept>
#include <vector>

#define CS_PIN 5

const size_t MOTOR_COUNT = 8;
const size_t BEV_COUNT = 12;

const size_t PASSCODE[] = {1,0,2,5}; //ik im not proud of this 

struct Bottle {
    String name;
    bool active = false;
    bool isShot = false;
};



class TouchControl {
    private:
        void touchOutput(String str); //touch screen requires specific format
        void flushStream() {
            Serial2.write(0xff);
            Serial2.write(0xff);
            Serial2.write(0xff);
        };
    public:
    /*
        TODO: create a function that abstracts the controlling of screen, like buttonId, etc. 
    */
        String checkForInput();

        void printDebug(String str) {
            Serial.print("SENT T.SCREEN: ");
            Serial.println(str);
            touchOutput(str);
        }; //will print to both console serial and to lcd serial
        void print(String str) {
            touchOutput(str);
        }; //void print()
        /* void controlCurPage:
        Follow Nextion guidelines.
        */
        void controlCurPage(String item, String cmd, String val);
        void changePage(String pageNum) {
            flushStream();
            String send = "page ";
            send.concat(pageNum);
            touchOutput(send);
        }; //void changePage()

}; //class TouchControl

class Beverage {
    private:
        bool isActive = true;
        String name;
        double ozArr[MOTOR_COUNT];
        String additionalInstructions = "";
    public:
        void createBeverage(std::vector<Bottle> &bottles);
};

class Machine {
    private:
        //SD CARD
        File config;
        File log;
        //File paymentLog
        std::vector<Bottle> bottles;
        std::vector<Beverage> beverages;
        void initSD();

    public:
        TouchControl touchscreen;
        void boot();
        void makeSelection();
        
};


#endif /*Machine_hpp*/