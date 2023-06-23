#include "Machine.hpp"

void Machine::boot()
{
    Serial.begin(115200);
    Serial.println("Serial 1 init on ");
    Serial2.begin(115200);
    Serial.println("Serial 2 init on 115200");

    touchscreen.changePage("0"); // change to boot page
    touchscreen.controlCurPage("t3", "txt", "Serial 2 connected on 115200");

    // reserve and fill vectors
    bottles.reserve(MOTOR_COUNT);
    beverages.reserve(BEV_COUNT);
    // TODO: These need to be filled from SD Card data
    try
    {
        initSD();
        initConfig();
        initBottles();
        initData();
        loadCell.initLoadCell();
        touchscreen.controlCurPage("t3", "txt", "Scale init.");
    } // try
    catch (String msg)
    {
        Serial.println(msg);
        touchscreen.controlCurPage("t3", "txt", msg);
        // TODO: trigger reboot w/delay
    } // catch msg
    catch (std::exception &e)
    {
        touchscreen.controlCurPage("t3", "txt", "ERROR 100: GENERIC EXCEPTION THROWN, BOOT FAILED.");
        Serial.println(e.what());
        // TODO: trigger reboot w/delay

    } // catch
    

    Serial.println("Boot successful.");
    Serial.println("authCocktail: " + String(authCocktail) + " authShots: " + String(authShots));

    Serial.println("ONBOARD BOTTLE DATA READ: ");
    for (auto bottle : bottles)
    {
        Serial.println(bottle.name);
        Serial.println(bottle.id);
        Serial.println(bottle.active);
        Serial.println(bottle.isShot);
        Serial.println(bottle.costPerOz);
        Serial.println(bottle.estimatedCapacity);
        Serial.println();
    } // for every bottle
    Serial.println();
    Serial.println("ONBOARD BOTTLE BEV READ: ");
    for (auto bev : beverages)
    {
        Serial.println(bev.name);
        Serial.println(bev.isActive);
        for (size_t i = 0; i < MOTOR_COUNT; i++)
        {
            Serial.print(bev.ozArr[i]);
            Serial.print(" ");
        } // for i
        Serial.println();
        Serial.println(bev.additionalInstructions);
        Serial.println();
    } // for every bev

    /*
    try calibrating, if cancelled or failed, try again, then reboot.
    */
    calibrate();

    loadMainMenu();
} // Machine::boot()

void Machine::initSD()
{
    // BUG: Not handling errors. If there is an issue with .begin() it will just keep rebooting without info provided to user.
    if (!SD.begin(CS_PIN))
    {
        throw("ERROR 101: SD CARD FAILED INIT.");
    } // if SD init failed
    config = SD.open("/config.txt");
    if (!config)
    {
        throw("ERROR 102: config.txt FILE ERROR.");
    } // if config fail
    data = SD.open("/data.txt");
    if (!data)
    {
        throw("ERROR 102: data.txt FILE ERROR.");
    } // if log fail
    log = SD.open("/log.txt");
    if (!log)
    {
        throw("ERROR 102: log.txt FILE ERROR.");
    } // if log fail
    log.close();
} // Machine::initSD()

void Machine::initConfig()
{
    // Read authCocktail
    String line = config.readStringUntil('\n');
    authCocktail = line.substring(line.indexOf('=') + 1).toInt();
    line = config.readStringUntil('\n');
    authShots = line.substring(line.indexOf('=') + 1).toInt();
    config.close();
} // Machine::initConfig()

void Machine::initBottles()
{
    for (size_t i = 0; i < MOTOR_COUNT; i++)
    {
        String path = "/bottle-" + String(i) + ".txt";
        File curBottle = SD.open(path.c_str());
        if (!curBottle)
        {
            String msg = "ERROR 102: " + path + " FILE ERROR.";
            throw(msg);
        } // if file error
        Bottle bottle;
        bottle.name = curBottle.readStringUntil('\n');
        bottle.id = curBottle.readStringUntil('\n').toInt();
        int bottleIsActive = curBottle.readStringUntil('\n').toInt();
        if (bottleIsActive != 1 && bottleIsActive != 0)
        {
            String error = "FILE ERROR Bottle Index: " + String(i) + " invalid type isActive bool.";
            throw(error);
        } // if invalid type
        int bottleIsShot = curBottle.readStringUntil('\n').toInt();
        if (bottleIsShot != 1 && bottleIsShot != 0)
        {
            String error = "FILE ERROR Bottle Index: " + String(i) + " invalid type isShot bool.";
            throw(error);
        } // if invalid type
        bottle.active = bottleIsActive;
        bottle.isShot = bottleIsShot;
        bottle.costPerOz = curBottle.readStringUntil('\n').toDouble();
        bottle.estimatedCapacity = curBottle.readStringUntil('\n').toDouble();
        bottles.push_back(bottle);
        curBottle.close();
    } // for i
} // Machine::initBottles()

void Machine::initData()
{
    for (size_t i = 0; i < BEV_COUNT; i++)
    {
        Beverage bev;
        bev.name = data.readStringUntil('\n');
        bev.id = data.readStringUntil('\n').toInt();
        int bevIsActive = data.readStringUntil('\n').toInt();
        if (bevIsActive != 1 && bevIsActive != 0)
        {
            String error = "FILE ERROR Beverage Index: " + String(i) + " invalid type isActive bool.";
            throw(error);
        } // if invalid type
        bev.isActive = bevIsActive;

        std::string ozLine = data.readStringUntil('\n').c_str();
        std::stringstream ss(ozLine);
        for (size_t j = 0; j < MOTOR_COUNT; j++)
        {
            ss >> bev.ozArr[j];
        } // for j

        String inst = data.readStringUntil('\n');
        inst.trim();
        if (inst == "null")
        {
            inst = "";
        } // if (inst == "null")
        bev.additionalInstructions = inst;
        beverages.push_back(bev);
    } // for i
    data.close();
} // Machine::initData()

InputData Machine::checkForInput()
{
    InputData input;
    if (Serial2.available())
    {
        String inputStr = Serial2.readString();
        if (inputStr.isEmpty())
        {
            return input;
        }
        Serial.println("Data from display: " + inputStr); // print the received data
        inputStr.trim();                                  // remove any whitespace

        String printableStr = "";
        for (int i = 0; i < inputStr.length(); i++)
        {
            if (isPrintable(inputStr.charAt(i)))
            {
                printableStr += inputStr.charAt(i);
            }
        }

        int separatorIndex = printableStr.indexOf('@');

        // if we didn't find the '@' symbol, throw an error
        if (separatorIndex == -1)
        {
            Serial.println("Expected a '@' in the command, but received: " + printableStr);
            return input;
        }

        // split the command into two parts
        String command = printableStr.substring(1, separatorIndex); // extract the command from the input string
        command.trim();                                             // remove any whitespace

        String idStr = printableStr.substring(separatorIndex + 1, separatorIndex + 3);
        idStr.trim(); // remove any whitespace

        // convert the ID to an integer
        int id = idStr.toInt();

        // store the command and ID in the struct
        input.cmd = command;
        input.id = id;
    }
    return input;
}

void TouchControl::touchOutput(String str)
{
    for (size_t i = 0; i < str.length(); i++)
    {
        Serial2.write(str[i]);
    } // for i

    flushStream();

} // TouchControl::touchOutput

void TouchControl::controlCurPage(String item, String cmd, String val)
{
    flushStream();
    String send = "";
    send.concat(item);
    send.concat('.');
    send.concat(cmd);
    send.concat('=');
    if (cmd == "txt")
    {
        send.concat("\"");
        send.concat(val);
        send.concat("\"");
    } // if txt
    else
    {
        send.concat(val);
    } // else

    printDebug(send);
} // TouchControl::controlCurPage()

void Machine::countdownMsg(int timeMillis, bool displayCountdown, String item, String destPageName)
{
    int countdown = timeMillis; // replace 10 with the number of seconds you want to wait
    unsigned long startTime = millis();
    int countdownSeconds = countdown / 1000;
    touchscreen.controlCurPage(item, "txt", "Returning to " + destPageName + " in " + String(countdownSeconds) + " seconds...");
    InputData dataIn = checkForInput();
    while (countdown > 0)
    {
        dataIn = checkForInput();
        if (dataIn.cmd == "cancel")
        {
            return;
        } // if cancel cmd
        unsigned long elapsedTime = millis() - startTime;
        if (elapsedTime >= 1000)
        {
            startTime += 1000;
            countdown -= 1000;
            countdownSeconds = countdown / 1000;
            touchscreen.controlCurPage(item, "txt", "Returning to " + destPageName + " in " + String(countdownSeconds) + " seconds...");
        } // if
    }     // while
} // TouchControl::countdownMsg()

void LoadScale::initLoadCell()
{
    LoadCell.begin(LOADCELL_GAIN);
    LoadCell.start(STABILIZING_TIME);
    LoadCell.setCalFactor(CALIBRATION_FACTOR);

} // LoadScale::initLoadCell()

void LoadScale::tareScale()
{
    // tareWeight = scale.read();
    Serial.println("RUNNING tareScale()");
    Serial.println("Current weight: " + String(LoadCell.getData()));
    // tareWeight = LoadCell.getData();
    LoadCell.tareNoDelay();
    Serial.println("Post tareNoDelay weight: " + String(LoadCell.getData()));
    // Serial.println("Tare weight: " + String(tareWeight));
} // LoadScale::tareScale()

int LoadScale::getCurrentWeight()
{

    // return scale.read_average(2);
    // return scale.read_average(2) - tareWeight;
    // return scale.read() - tareWeight;

    LoadCell.update();
    return LoadCell.getData();
} // LoadScale::getCurrentWeight()

void Machine::loadMainMenu()
{
    touchscreen.changePage("1");
    /*
    Check the status of each bottle, update UI accordingly.
    */
    // green color: 1024
    // yellow color: 65088
    // default color is green
    for (auto bottle : bottles)
    {
        if (!bottle.active)
        {
            String itemId = "bs" + String(bottle.id);
            touchscreen.controlCurPage(itemId, "bco", "65088");
        }
        else
        {
            String itemId = "bs" + String(bottle.id);
            touchscreen.controlCurPage(itemId, "bco", "1024");
        }
    } // for bottle
    if (debugMainPageOutput)
    {
        touchscreen.controlCurPage("debugTitle", "txt", "Debug Sys. Output: ");
        String debugDataStr;
        // debugDataStr.concat()
        touchscreen.controlCurPage("debugData", "txt", "No Data, Implement");
    } // if debugMainPageOutput
} // Machine::loadMainMenu()

void Machine::loadAdminMenu()
{
    /*
    Update auth statuses, update motor status, estimated remaining.
    TODO: beverage menu slider control w/ enable/disable, pricing updates, some sort of debug console out, access to primitive logs
    */
    touchscreen.changePage("4");

    touchscreen.controlCurPage("cauthsw", "val", String(authCocktail));
    touchscreen.controlCurPage("sauthsw", "val", String(authShots));

    for (auto bottle : bottles)
    {
        String itemId = "sw" + String(bottle.id);
        touchscreen.controlCurPage(itemId, "val", String(bottle.active));
        itemId = "bcap" + String(bottle.id);
        touchscreen.controlCurPage(itemId, "txt", String(bottle.estimatedCapacity));
    } // for bottle

} // Machine::loadAdminMenu()

void Machine::loadCocktailMenu()
{
    touchscreen.changePage("7");

    // for every beverage up to 15 per page, update the text buttons
    for (auto beverage : beverages)
    {
        String itemId = "b" + String(beverage.id);

        touchscreen.controlCurPage(itemId, "txt", beverage.name);
        if (beverage.isActive)
        {
            touchscreen.controlCurPage(itemId, "bco", "65088");
        } // if bev active
    }     // for each beverage
} // Machine::loadCocktailMenu()

void Machine::inputDecisionTree()
{
    InputData input = checkForInput(); // call the checkForInput() function to get the input data

    if (!input.cmd.isEmpty())
    {
        Serial.print("Received command: ");
        Serial.println(input.cmd);
        Serial.print("Received ID: ");
        Serial.println(input.id);

        // perform the appropriate action based on the command and ID
        if (input.cmd == "gopage")
        {
            // try to convert the page number to an integer
            int pageNumber = input.id;

            // if the page number is not between 0 and 99, throw an error
            if (pageNumber < 0 || pageNumber > 99)
            {
                Serial.println("Page number must be between 0 and 99, but received: " + String(pageNumber));
                return;
            }

            // if we got this far, everything is good, so print the page number
            Serial.print("Going to page ");
            Serial.println(pageNumber);

            switch (pageNumber)
            {
            case 1:
                loadMainMenu();
                break;
            case 4:
                loadAdminMenu();
                break;
            case 7:
                loadCocktailMenu();
                break;

            // add more cases here for other page numbers
            default:
                Serial.println("Invalid page number: " + String(pageNumber));
                break;
            }
        } // if gopage
        else if (input.cmd == "bev")
        {
            int bevId = input.id;
            Serial.println("Received beverage command: " + String(bevId));
            try
            {
                createBeverage(bevId);
            }
            catch (BeverageDisabledException &e)
            { //63488 red
                touchscreen.controlCurPage("t0", "pco", "63488");
                touchscreen.controlCurPage("t0", "txt", e.what());
                touchscreen.controlCurPage("t2", "txt", "");
                countdownMsg(COUNTDOWN_MSG_MS, true, "t1", "beverage menu");
                loadCocktailMenu();
            }
            catch (BottleDisabledException &e)
            {
                touchscreen.controlCurPage("t0", "pco", "63488");
                touchscreen.controlCurPage("t0", "txt", e.what());
                touchscreen.controlCurPage("t2", "txt", "");
                countdownMsg(COUNTDOWN_MSG_MS, true, "t1", "beverage menu");
                // TODO: Handle data change. Disable beverage that was called with this bottle.
                loadCocktailMenu();
            }
            catch (CupNotFoundException &e)
            {
                touchscreen.controlCurPage("t0", "pco", "63488");
                touchscreen.controlCurPage("t0", "txt", e.what());
                touchscreen.controlCurPage("t2", "txt", "");
                countdownMsg(COUNTDOWN_MSG_MS, true, "t1", "beverage menu");
                loadCocktailMenu();
            }
            catch (MotorTimeoutException &e)
            {
                touchscreen.controlCurPage("t0", "pco", "63488");
                touchscreen.controlCurPage("t0", "txt", e.what());
                touchscreen.controlCurPage("t2", "txt", "");
                countdownMsg(COUNTDOWN_MSG_MS, true, "t1", "beverage menu");
                // TODO: Handle data change. Disable bottle that was called with this beverage.
                loadCocktailMenu();
            }
            catch (CupRemovedException &e)
            {
                touchscreen.controlCurPage("t0", "pco", "63488");
                touchscreen.controlCurPage("t0", "txt", e.what());
                touchscreen.controlCurPage("t2", "txt", "");
                countdownMsg(COUNTDOWN_MSG_MS, true, "t1", "beverage menu");
                // TODO: Handle data. Write the total cost from this beverage to log.
                // e.get_priceDispensed();
                Serial.println("Cup Removed Exception in inputDecisionTree. TotalCost: " + String(e.get_priceDispensed()));
                loadCocktailMenu();
            }
            catch (BeverageCancelledException &e)
            {
                touchscreen.controlCurPage("t0", "pco", "63488");
                touchscreen.controlCurPage("t0", "txt", e.what());
                touchscreen.controlCurPage("t2", "txt", "");
                countdownMsg(COUNTDOWN_MSG_MS, true, "t1", "beverage menu");
                // TODO: Handle data. Write the total cost from this beverage to log.
                // e.get_priceDispensed();
                Serial.println("Beverage Cancelled Exception in inputDecisionTree. TotalCost: " + String(e.get_priceDispensed()));
                loadCocktailMenu();
            }
        } // elif bev
        else
        {
            Serial.println("Received an invalid command: " + input.cmd);
        }
    } // if cmd not empty
}

void Machine::createBeverage(int id)
{
    touchscreen.changePage("8");
    if (!beverages[id].isActive)
    {
        std::string error = "Beverage " + std::to_string(id) + " is not active.";
        throw BeverageDisabledException(error);
    } // if (beverages[id].isActive])

    Beverage &bev = beverages[id];

    touchscreen.controlCurPage("t0", "txt", bev.name);
    touchscreen.controlCurPage("t2", "txt", "!Don't touch scale until completion!");
    touchscreen.controlCurPage("t2", "pco", "33808");

    double estimatedCost = 0.0;

    for (int i = 0; i < MOTOR_COUNT; i++)
    {
        if (bev.ozArr[i] > 0 && !bottles[i].active)
        {
            throw BottleDisabledException("Bottle " + std::to_string(i) + " is not active.");
        }                                                                        // if (!bottles[i].active)
        if (bev.ozArr[i] > 0 && bottles[i].estimatedCapacity - 2 < bev.ozArr[i]) // if there is not enough liquid, -2 room for error
        {
            throw BottleDisabledException("Bottle " + std::to_string(i) + " not enough capacity.");
        } // if (bottles[i] <= bev.ozArr[i])

        estimatedCost += bev.ozArr[i] * bottles[i].costPerOz;
    } // for i

    // TODO: present cost to user, ask for confirmation and continue (also write to log and print that no profit is made, at cost dispensed) "Liquor is expensive, this cost is only the store cost of the liquor, no profit is made. Continue?"

    // check for cup w/scale
    int startWeight = loadCell.getCurrentWeight();
    if (startWeight < 4)
    {
        Serial.println("EXCEPTION: No cup found on scale: " + String(startWeight));
        throw CupNotFoundException("No cup found on scale.");
    } // if

    /*Loop through each bottle and determine amount to dispense.*/
    double totalCost = 0.0;
    for (int i = 0; i < MOTOR_COUNT; i++)
    {
        if (bev.ozArr[i] > 0)
        {
            double actualDispensed = 0.0;
            touchscreen.controlCurPage("t1", "txt", "Dispensing " + bottles[i].name + "...");
            try
            {
                actualDispensed = dispense(i, bev.ozArr[i]);
            }
            catch (CupRemovedException &e)
            {
                actualDispensed = e.get_dispensed_oz();
                bottles[i].estimatedCapacity -= actualDispensed;
                totalCost += actualDispensed * bottles[i].costPerOz;
                e.set_priceDispensed(totalCost);
                throw e;
            }
            catch (BeverageCancelledException &e)
            {
                actualDispensed = e.get_dispensed_oz();
                bottles[i].estimatedCapacity -= actualDispensed;
                totalCost += actualDispensed * bottles[i].costPerOz;
                e.set_priceDispensed(totalCost);
                throw e;
            }
            double unitCost = actualDispensed * bottles[i].costPerOz;
            bottles[i].estimatedCapacity -= actualDispensed;
            totalCost += unitCost;
        } // if

    } // for i

    /*TODO: Dispensing complete, prompt user with completion and additional instructions.*/
    touchscreen.controlCurPage("t0", "pco", "1024");
    touchscreen.controlCurPage("t0", "txt", "Dispensing complete!");
    touchscreen.controlCurPage("t1", "txt", bev.additionalInstructions);
    countdownMsg(COUNTDOWN_BEV_MSG_MS, true, "t2", "main menu");
    loadMainMenu();
    return;
} // Machine::createBeverage()

double Machine::dispense(int motorId, double oz)
{
    double beforeDispense = loadCell.getCurrentWeight();
    Serial.println("Before dispense: " + String(beforeDispense));
    double goalDispense = convertToScaleUnit(oz) + goalDispense;
    Serial.println("Goal Dispense: " + String(goalDispense));
    double currentDispense = beforeDispense;

    long startRunTime = millis();
    InputData dataIn = checkForInput();
    while (currentDispense < goalDispense)
    {
        // TODO: Keep checking for user input, if its cancel then terminate
        if (dataIn.cmd == "cancel")
        {
            // TODO: STOP MOTOR
            throw BeverageCancelledException("Cancelled by user.", convertToOz(currentDispense - beforeDispense));
        } // if
        dataIn = checkForInput();
        // TODO: RUN MOTOR (Get the serial digital output running) (write on)
        currentDispense = loadCell.getCurrentWeight();
        long curTime = millis();
        if (curTime - startRunTime > MOTOR_TIMEOUT_MS)
        {
            // TODO: STOP MOTOR
            throw MotorTimeoutException("Motor " + std::to_string(motorId) + " timed out at " + std::to_string(curTime - startRunTime) + " milliseconds and at  " + std::to_string(currentDispense) + " oz.");
        } // if
        // if someone suddenly removes the cup (weight decreases substantially) the loop should terminate to avoid mess
        if (currentDispense < beforeDispense - CUP_REMOVAL_THRESHOLD)
        {
            // throw CupRemovedException with a string that contains some detail about the weight
            // TODO: STOP MOTOR
            // throw CupRemovedException("Cup removed during dispensing. Before: " + std::to_string(beforeDispense) + " After: " + std::to_string(currentDispense) + "Const Cup Removal Threshold: " + std::to_string(CUP_REMOVAL_THRESHOLD));
            throw CupRemovedException("Cup removed during dispensing.", convertToOz(currentDispense - beforeDispense));
        } // if
        if (debugPrintWeightSerialDispense) {
            Serial.println("Current Dispense: " + String(currentDispense));
        } //if

    } // while
    // TODO: STOP MOTOR
    return convertToOz(currentDispense - beforeDispense);
} // Machine::dispense()

double Machine::convertToScaleUnit(double oz)
{
    return oz * SCALE_OZ_FACTOR;
} // Machine::convertToScaleUnit()

double Machine::convertToOz(double scaleUnit)
{
    return scaleUnit / SCALE_OZ_FACTOR;
} // Machine::convertToOz()

void Machine::calibrate()
{

    // Load Calibrate Page
    touchscreen.changePage("8"); // Using the dispense page
    touchscreen.controlCurPage("t0", "txt", "Calibrating...");
    touchscreen.controlCurPage("t1", "txt", "!!CLEAR, DON'T TOUCH SCALE!!");
    /*
    Print out a message about calibration. (Clear the scale etc.)
    FUTURE: If the scale always outputs a raw value, maybe save it so when we calibrate it should be close to that.
    */

    // timed monitored delay, keep checking for input during the delay
    Serial.println("CALIBRATE() Waiting " + String(CALIBRATION_WAIT_MS) + " milliseconds for user to cancel.");
    unsigned long startTime = millis();
    InputData dataIn = checkForInput();
    while (millis() - startTime < CALIBRATION_WAIT_MS)
    {
        if (dataIn.cmd == "cancel")
        {
            Serial.println("Calibration cancelled.");
            Serial.println("Current weight: " + String(loadCell.getCurrentWeight()));
            return;
        }
        dataIn = checkForInput();
    } // while

    loadCell.tareScale();

    loadMainMenu();
} // Machine::calibrate()
