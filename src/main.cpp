#include "Machine.hpp"
#include <Arduino.h>

Machine bevMaker(MOTOR_COUNT, BEV_COUNT);

void setup()
{
    bevMaker.boot();
}

void loop()
{
    /*
    if there is serial input (either from debug console or from touchscreen), run the decision tree
    */
    try
    {
        bevMaker.inputDecisionTree();
    }
    catch (const std::invalid_argument &e)
    {
        Serial.println(e.what());
    }
    catch (...)
    {
        Serial.println("An unexpected error occurred.");
    }

    bevMaker.loadCell.LoadCell.update(); // TODO move LoadScale out of class and into machine for this reason

    if (bevMaker.debugPrintWeightSerial)
    {
        Serial.println("Weight: " + String(bevMaker.loadCell.getCurrentWeight()));
    }

    // if (!bevMaker.mqttClient.connected())
    // {
    //     bevMaker.connectMqtt();
    // }
    bevMaker.mqttClient.loop();
}
