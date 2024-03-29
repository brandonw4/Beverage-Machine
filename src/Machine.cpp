#include "Machine.hpp"

Machine::Machine(size_t bottleCount, size_t bevCount)
{
    bottles.reserve(bottleCount);
    beverages.reserve(bevCount);
} // Machine::Machine()

void Machine::boot()
{
    Serial.begin(115200);
    Serial.println("Serial 1 init on 115200");
    Serial2.begin(115200);
    Serial.println("Serial 2 init on 115200");
    frontLED.init();
    frontLED.standby();

    touchscreen.changePage("0"); // change to boot page
    touchscreen.controlCurPage("t3", "txt", "Attempting SD Load.");

    try
    {
        initSD();
        touchscreen.controlCurPage("t3", "txt", "SD Load Successful.");
        // initConfig();
        // initBottles();
        initData();
        touchscreen.controlCurPage("t3", "txt", "SD Data Init.");
        loadCell.initLoadCell();
        touchscreen.controlCurPage("t3", "txt", "Scale init.");
        initWifi();
        initMqtt();
        // saveAllNVSBevs();
        // saveAllNVSBottles();

        initNVSBottles();
        touchscreen.controlCurPage("t3", "txt", "NVS Bottles init.");
        initNVSBev();
        touchscreen.controlCurPage("t3", "txt", "NVS Bevs. init.");
        motorControl.init();
        touchscreen.controlCurPage("t3", "txt", "Motor Control init.");
        txMachineStatus();

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
        Serial.println(bottle.totalCapacity);
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
        bottle.totalCapacity = curBottle.readStringUntil('\n').toDouble();
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

void Machine::initNVSBottles()
{
    preferences.begin("bottles", true);
    for (size_t i = 0; i < MOTOR_COUNT; i++)
    {
        String key = "bottle-" + String(i);
        String json = preferences.getString(key.c_str());
        if (json.isEmpty())
        {
            std::string msg = "ERROR: Bottle " + std::to_string(i) + " not found in NVS.";
            throw NVSNotFound(msg);
        } // if json empty
        Bottle bottle = Bottle::fromJson(json);
        bottles.push_back(bottle);
    }
    preferences.end();
} // Machine::initNVSBottles()

void Machine::initNVSBev()
{
    preferences.begin("beverages", true);
    for (size_t i = 0; i < BEV_COUNT; i++)
    {
        String key = "bev-" + String(i);
        String json = preferences.getString(key.c_str());
        if (json == "")
        {
            std::string msg = "ERROR: Beverage " + std::to_string(i) + " not found in NVS.";
            throw NVSNotFound(msg);
        }
        Beverage bev = Beverage::fromJson(json);
        beverages.push_back(bev);
    }
    preferences.end();
} // Machine::initNVSBev()

void Machine::saveNVSBottle(int bottleId, const Bottle &bottle)
{
    preferences.begin("bottles", false); // false indicates write (not readonly)
    String key = "bottle-" + String(bottleId);
    preferences.putString(key.c_str(), bottle.toJson());
    preferences.end();
} // Machine::saveNVSBottle()

void Machine::saveAllNVSBottles()
{
    // save all bottles to NVS
    for (auto bottle : bottles)
    {
        saveNVSBottle(bottle.id, bottle);
    }
} // Machine::saveAllNVSBottles()

void Machine::saveNVSBev(int bevId, const Beverage &bev)
{
    preferences.begin("beverages", false); // false indicates write (not readonly)
    String key = "bev-" + String(bevId);
    preferences.putString(key.c_str(), bev.toJson());
    preferences.end();
} // Machine::saveNVSBev()

void Machine::saveAllNVSBevs()
{
    // save all beverages to NVS
    for (auto bev : beverages)
    {
        saveNVSBev(bev.id, bev);
    }
} // Machine::saveAllNVSBevs()

void Machine::loopCheckSerial()
{
    std::vector<InputData> inputs = checkForInput(); // call the checkForInput() function to get the input data
    if (inputs[0].cmd.isEmpty())
    {
        return;
    }                          // if cmd empty
    inputDecisionTree(inputs); // call the inputDecisionTree() function to process the input data
} // Machine::loopCheckSerial()

std::vector<InputData> Machine::checkForInput()
{

    if (!Serial2.available())
    {
        return std::vector<InputData>({InputData()});
    }

    String inputStr;
    // read the chars (char)Serial2.read() into the inputStr
    while (Serial2.available())
    {
        inputStr += (char)Serial2.read();
    } // while

    if (inputStr.isEmpty())
    {
        return std::vector<InputData>({InputData()});
    }
    return formatInputString(inputStr);
}

std::vector<InputData> Machine::formatInputString(String inputStr)
{
    std::vector<InputData> inputs;
    InputData input;
    inputs.push_back(input);
    // if short command format
    if (inputStr[0] == '!')
    {
        Serial.println("Short command: " + inputStr); // print the received data
        inputStr.trim();                              // remove any whitespace

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
            return inputs;
        }

        // split the command into two parts
        String command = printableStr.substring(1, separatorIndex); // extract the command from the input string
        command.trim();                                             // remove any whitespace

        // check if the command is "edCap"
        if (currentMotorEditId != -1 && command == "edCap")
        {
            // split the input string into multiple parts using the '&' symbol as a separator
            String edEstCapStr = "";
            String edTotalCapStr = "";
            String edPPOzStr = "";
            int separatorIndex1 = printableStr.indexOf('&');
            int separatorIndex2 = printableStr.indexOf('&', separatorIndex1 + 1);
            if (separatorIndex1 != -1 && separatorIndex2 != -1)
            {
                edEstCapStr = printableStr.substring(separatorIndex + 1, separatorIndex1);
                edTotalCapStr = printableStr.substring(separatorIndex1 + 1, separatorIndex2);
                edPPOzStr = printableStr.substring(separatorIndex2 + 1);
            }

            // extract the values for edEstCap, edTotalCap, and edPPOz from the resulting substrings
            double edEstCap = edEstCapStr.substring(edEstCapStr.indexOf('=') + 1).toDouble();
            double edTotalCap = edTotalCapStr.substring(edTotalCapStr.indexOf('=') + 1).toDouble();
            double edPPOz = edPPOzStr.substring(edPPOzStr.indexOf('=') + 1).toDouble();

            // do something with the extracted values
            Serial.print("Received edCap command with values, bottle ID " + String(currentMotorEditId) + ": ");
            Serial.print("edEstCap = ");
            Serial.print(edEstCap);
            Serial.print(", edTotalCap = ");
            Serial.print(edTotalCap);
            Serial.print(", edPPOz = ");
            Serial.println(edPPOz);

            // update the bottle's estimated capacity, total capacity, and cost per oz
            if (edEstCap != -1)
            {
                bottles[currentMotorEditId].estimatedCapacity = edEstCap;
            } // if changed
            if (edTotalCap != -1)
            {
                bottles[currentMotorEditId].totalCapacity = edTotalCap;
            } // if changed
            if (edPPOz != -1)
            {
                bottles[currentMotorEditId].costPerOz = edPPOz;
            } // if changed

            // reload the page
            loadMotorEditMenu(currentMotorEditId);

            // send the status to cloud
            txMachineStatus();
            saveAllNVSBottles();

            return inputs;
        } // if command == edCap

        String valStr = printableStr.substring(separatorIndex + 1, separatorIndex + 3);

        // store the command and ID in the struct
        inputs[0].cmd = command;
        inputs[0].value = valStr;
    } // if short command format
    // if long command format
    else if (inputStr[0] == '%')
    {
        /*LONG COMMAND FORMAT: vector index 0 will only store the identifying command for use in the decision tree.
        The rest of the indicies will contain the InputData following a filled command, value format.*/
        Serial.println("Long command: " + inputStr); // print the received data
        inputStr.trim();                             // remove any whitespace
        std::string fullString = inputStr.substring(1).c_str();
        std::istringstream commandStream(fullString);
        std::string mainCommand;
        std::getline(commandStream, mainCommand, '@');

        inputs[0].cmd = mainCommand.c_str();
        Serial.println("Main Long Command: " + inputs[0].cmd);

        // for the rest of the command/arg pairs
        std::string commandArgPair;
        while (std::getline(commandStream, commandArgPair, '&'))
        {
            std::istringstream pairStream(commandArgPair);
            std::string command;
            std::getline(pairStream, command, '=');
            std::string arg;
            std::getline(pairStream, arg);
            inputs.push_back({command.c_str(), arg.c_str()});

            // DELETE for speed
            String debugCmdOut = command.c_str();
            String debugArgOut = arg.c_str();
            Serial.println("Command: " + debugCmdOut + " Arg: " + debugArgOut);
        } // while

    } // if long command format

    return inputs;
} // Machine::formatInputString()

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
    InputData dataIn = checkForInput()[0];
    while (countdown > 0)
    {
        dataIn = checkForInput()[0];
        mqttClient.loop();
        wifiClient.flush();

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
    // grey color: 33808
    // default color is green
    for (auto bottle : bottles)
    {
        String itemId = "bs" + String(bottle.id);
        touchscreen.controlCurPage(itemId, "txt", bottle.name);
        if (!bottle.active)
        {
            touchscreen.controlCurPage(itemId, "bco", "33808");
        }
        else
        {
            touchscreen.controlCurPage(itemId, "bco", "1024");
        }
    } // for bottle
    if (!isCalibrated)
    {
        touchscreen.controlCurPage("t11", "txt", "Not Calibrated");
        touchscreen.controlCurPage("t11", "bco", "63488");
    }
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

} // Machine::loadAdminMenu()

void Machine::loadMotorControlMenu()
{
    touchscreen.changePage("9");

    for (auto bottle : bottles)
    {
        String itemId = "sw" + String(bottle.id);
        touchscreen.controlCurPage(itemId, "val", String(bottle.active));
        itemId = "bname" + String(bottle.id);

        touchscreen.controlCurPage(itemId, "txt", String(bottle.id) + ":" + bottle.name);
        itemId = "bcap" + String(bottle.id);
        touchscreen.controlCurPage(itemId, "txt", String(bottle.estimatedCapacity));
        itemId = "btcap" + String(bottle.id);
        touchscreen.controlCurPage(itemId, "txt", String(bottle.totalCapacity));
    } // for bottle
} // Machine::loadMotorControlMenu()

void Machine::loadMotorEditMenu(int motorId)
{
    touchscreen.changePage("12");
    touchscreen.controlCurPage("t0", "txt", String(motorId) + ":" + bottles[motorId].name);
    touchscreen.controlCurPage("curEstCap", "txt", String(bottles[motorId].estimatedCapacity));
    touchscreen.controlCurPage("curTotalCap", "txt", String(bottles[motorId].totalCapacity));
    touchscreen.controlCurPage("curPPOz", "txt", String(bottles[motorId].costPerOz));
    currentMotorEditId = motorId;
} // Machine::loadMotorEditMenu()

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

void Machine::inputDecisionTree(std::vector<InputData> &inputs)
{
    if (inputs[0].cmd.isEmpty())
    {
        return;
    } // if cmd empty

    if (inputs.size() <= 1) // if the command is short form, the vector should have no more than 1
    {
        InputData input = inputs[0]; // get the first element of the vector
        Serial.print("Received command: ");
        Serial.println(input.cmd);
        Serial.print("Received ID: ");
        Serial.println(input.value);

        // perform the appropriate action based on the command and ID
        if (input.cmd == "gopage")
        {
            // try to convert the page number to an integer
            int pageNumber = input.value.toInt();

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
            case 9:
                loadMotorControlMenu();
                break;

            // add more cases here for other page numbers
            default:
                Serial.println("Invalid page number: " + String(pageNumber));
                break;
            }
        } // if gopage
        else if (input.cmd == "bev")
        {
            int bevId = input.value.toInt();
            Serial.println("Received beverage command: " + String(bevId));
            try
            {
                createBeverage(bevId);
            }
            catch (BeverageDisabledException &e)
            { // 63488 red
                touchscreen.controlCurPage("t0", "pco", "63488");
                touchscreen.controlCurPage("t0", "txt", e.what());
                touchscreen.controlCurPage("t2", "txt", "");
                frontLED.errorAnimation();
                countdownMsg(COUNTDOWN_MSG_MS, true, "t1", "beverage menu");
                loadCocktailMenu();
            }
            catch (MachineNotCalibratedException &e)
            {
                touchscreen.controlCurPage("t0", "pco", "63488");
                touchscreen.controlCurPage("t0", "txt", e.what());
                touchscreen.controlCurPage("t2", "txt", "");
                frontLED.errorAnimation();
                countdownMsg(COUNTDOWN_MSG_MS, true, "t1", "beverage menu");
                loadCocktailMenu();
            }

            catch (BottleDisabledException &e)
            {
                touchscreen.controlCurPage("t0", "pco", "63488");
                touchscreen.controlCurPage("t0", "txt", e.what());
                touchscreen.controlCurPage("t2", "txt", "");
                frontLED.errorAnimation();
                countdownMsg(COUNTDOWN_MSG_MS, true, "t1", "beverage menu");
                // TODO: Handle data change. Disable beverage that was called with this bottle.
                loadCocktailMenu();
            }
            catch (CupNotFoundException &e)
            {
                touchscreen.controlCurPage("t0", "pco", "63488");
                touchscreen.controlCurPage("t0", "txt", e.what());
                touchscreen.controlCurPage("t2", "txt", "");
                frontLED.errorAnimation();
                countdownMsg(COUNTDOWN_MSG_MS, true, "t1", "beverage menu");
                loadCocktailMenu();
            }
            catch (MotorTimeoutException &e)
            {
                touchscreen.controlCurPage("t0", "pco", "63488");
                touchscreen.controlCurPage("t0", "txt", e.what());
                touchscreen.controlCurPage("t2", "txt", "");
                frontLED.errorAnimation();
                countdownMsg(COUNTDOWN_MSG_MS, true, "t1", "beverage menu");
                // TODO: Handle data change. Disable bottle that was called with this beverage.
                loadCocktailMenu();
            }
            catch (CupRemovedException &e)
            {
                touchscreen.controlCurPage("t0", "pco", "63488");
                touchscreen.controlCurPage("t0", "txt", e.what());
                touchscreen.controlCurPage("t2", "txt", "");
                frontLED.errorAnimation();
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
                frontLED.errorAnimation();
                countdownMsg(COUNTDOWN_MSG_MS, true, "t1", "beverage menu");
                // TODO: Handle data. Write the total cost from this beverage to log.
                // e.get_priceDispensed();
                Serial.println("Beverage Cancelled Exception in inputDecisionTree. TotalCost: " + String(e.get_priceDispensed()));
                loadCocktailMenu();
            }

            // regardless, save all bottle updates to NVS
            saveAllNVSBottles();
            txMachineStatus();
            frontLED.resetProgress();
            frontLED.standby();

        } // elif bev
        else if (input.cmd == "finish")
        {
            Serial.print("Received finish command ");
            Serial.println(input.value);

            if (input.value.toInt() == 12)
            {
                Serial.println("recieved page 12");
                currentMotorEditId = -1;
                loadMotorControlMenu();
            } // if page 12

        } // else if finishGo
        else if (input.cmd == "ebs")
        {
            Serial.print("Received ebs command ");
            int id = input.value.toInt();
            Serial.println(id);
            if (id < 0 || id > MOTOR_COUNT)
            {
                throw("Invalid ebs bottle id: " + String(id));
            } // if invalid id
            bottles[id].active = !bottles[id].active;
            saveNVSBottle(id, bottles[id]);
            txMachineStatus();
        } // if edit bottle status
        else if (input.cmd == "ebc")
        {
            Serial.print("Received ebc command ");
            int id = input.value.toInt();
            Serial.println(id);
            if (id < 0 || id > MOTOR_COUNT)
            {
                throw("Invalid ebc bottle id: " + String(id));
            } // if invalid id
            loadMotorEditMenu(id);
        } // else if ebc (editBottleCapacity)
        else if (input.cmd == "ss")
        {
            Serial.println("Received send status (ss) command");
            txMachineStatus();
        } // if send status (ss)
        else if (input.cmd == "cba")
        {
            Serial.println("Received calibrate command.");
            calibrate();
        } // if calibrate
        else
        {
            Serial.println("Received an invalid command: " + input.cmd);
        }
    } // if short form command
    else if (inputs.size() > 1)
    {
        String cmdType = inputs[0].cmd;
        Serial.println("Received long form command: " + cmdType);

        for (auto input : inputs)
        {
            Serial.println("Command: " + input.cmd + " Value: " + input.value);
        }
    } // if long form command
}

void Machine::createBeverage(int id)
{
    touchscreen.changePage("8");
    if (!beverages[id].isActive)
    {
        std::string error = "Beverage " + std::to_string(id) + " is not active.";
        throw BeverageDisabledException(error);
    } // if (beverages[id].isActive])

    if (!isCalibrated)
    {
        std::string error = "Machine is not calibrated.";
        throw MachineNotCalibratedException(error);
    }

    Beverage &bev = beverages[id];

    touchscreen.controlCurPage("t0", "txt", bev.name);
    touchscreen.controlCurPage("t2", "txt", "!Don't touch scale until completion!");
    touchscreen.controlCurPage("t2", "pco", "33808");

    double estimatedCost = 0.0;
    double totalOz = 0.0;

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
        totalOz += bev.ozArr[i];
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
    frontLED.clear();
    for (int i = 0; i < MOTOR_COUNT; i++)
    {
        if (bev.ozArr[i] > 0)
        {
            double actualDispensed = 0.0;
            touchscreen.controlCurPage("t1", "txt", "Dispensing " + bottles[i].name + "...");
            try
            {
                actualDispensed = dispense(i, bev.ozArr[i], actualDispensed, totalOz);
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
    frontLED.successAnimation();
    countdownMsg(COUNTDOWN_BEV_MSG_MS, true, "t2", "main menu");
    loadMainMenu();
    return;
} // Machine::createBeverage()

double Machine::dispense(int motorId, double oz, double runningOz, double totalOz)
{
    double beforeDispense = loadCell.getCurrentWeight();
    Serial.println("Before dispense: " + String(beforeDispense));
    double goalDispense = convertToScaleUnit(oz) + beforeDispense;
    Serial.println("Goal Dispense: " + String(goalDispense));
    double currentDispense = beforeDispense;

    long startRunTime = millis();
    InputData dataIn = checkForInput()[0];
    motorControl.runMotor(motorId, true);
    while (currentDispense < goalDispense)
    {
        mqttClient.loop();
        wifiClient.flush();

        if (dataIn.cmd == "cancel")
        {
            motorControl.runMotor(motorId, false);
            throw BeverageCancelledException("Cancelled by user.", convertToOz(currentDispense - beforeDispense));
        } // if
        dataIn = checkForInput()[0];
        currentDispense = loadCell.getCurrentWeight();

        // Calculate the dispensed amount in the current iteration
        double dispensedInThisIteration = convertToOz(currentDispense - beforeDispense);
        // Update LED Progress
        double completionPercentage = ((runningOz + dispensedInThisIteration) / totalOz) * 100;
        frontLED.updateProgress(completionPercentage);

        long curTime = millis();
        if (curTime - startRunTime > MOTOR_TIMEOUT_MS)
        {
            motorControl.runMotor(motorId, false);
            throw MotorTimeoutException("Motor " + std::to_string(motorId) + " timed out at " + std::to_string(curTime - startRunTime) + " milliseconds and at  " + std::to_string(currentDispense) + " oz.");
        } // if
        // if someone suddenly removes the cup (weight decreases substantially) the loop should terminate to avoid mess
        if (currentDispense < beforeDispense - CUP_REMOVAL_THRESHOLD)
        {
            // throw CupRemovedException with a string that contains some detail about the weight
            motorControl.runMotor(motorId, false);
            // throw CupRemovedException("Cup removed during dispensing. Before: " + std::to_string(beforeDispense) + " After: " + std::to_string(currentDispense) + "Const Cup Removal Threshold: " + std::to_string(CUP_REMOVAL_THRESHOLD));
            throw CupRemovedException("Cup removed during dispensing.", convertToOz(currentDispense - beforeDispense));
        } // if
        if (debugPrintWeightSerialDispense)
        {
            Serial.println("Current Dispense: " + String(currentDispense));
        } // if

    } // while

    motorControl.runMotor(motorId, false);

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
    InputData dataIn = checkForInput()[0];
    while (millis() - startTime < CALIBRATION_WAIT_MS)
    {
        if (dataIn.cmd == "cancel")
        {
            Serial.println("Calibration cancelled.");
            Serial.println("Current weight: " + String(loadCell.getCurrentWeight()));
            return;
        }
        dataIn = checkForInput()[0];
    } // while

    loadCell.tareScale();
    isCalibrated = true;

    loadMainMenu();
} // Machine::calibrate()

void Machine::initWifi()
{
    // TODO: read in wifi ssid from file
    const char *SSID = "TallDolphin";
    const char *PWD = "3138858442";
    Serial.println("Connecting to " + String(SSID) + "...");
    WiFi.begin(SSID, PWD);
    int wifiTimeout = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - wifiTimeout > WIFI_TIMEOUT_MS)
        {
            Serial.println("Wifi connection timed out.");
            throw WifiFailedInit("Wifi connection timed out. SSID: " + std::string(SSID) + " PWD: " + std::string(PWD));
        } // if
        delay(500);
        Serial.print(".");
    } // while
    Serial.println("Connected to: " + String(SSID) + " IP: " + String(WiFi.localIP()));
    touchscreen.controlCurPage("t3", "txt", "Init Wifi. SSID: " + String(SSID) + " IP: " + String(WiFi.localIP()));
    randomSeed(micros());

} // Machine::initWifi()

void Machine::initMqtt()
{
    wifiClient.setCACert((const char *)Secrets::CERT);
    mqttClient.setClient(wifiClient);
    mqttClient.setServer(Secrets::MQTT_SERVER, MQTT_PORT);
    std::function<void(char *, uint8_t *, unsigned int)> func = std::bind(&Machine::mqttCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    mqttClient.setCallback(func);
    mqttClient.setBufferSize(1024);
    try
    {
        connectMqtt();
    }
    catch (MqttFailedInit &e)
    {
        Serial.println(e.what());
    }
    touchscreen.controlCurPage("t3", "txt", "Init Mqtt.");
} // Machine::initMqtt()

void Machine::connectMqtt()
{
    int mqttTimeout = millis();
    Serial.println("Reconnecting to MQTT Broker..");
    while (!mqttClient.connected())
    {
        if (millis() - mqttTimeout > MQTT_TIMEOUT_MS)
        {
            Serial.println("MQTT connection timed out.");
            throw MqttFailedInit("MQTT connection timed out.");
        } // if

        String clientId = "ESP32Client-";
        clientId += String(random(0xffff), HEX);

        mqttClient.setKeepAlive(MQTT_KEEP_ALIVE);

        if (mqttClient.connect(clientId.c_str(), Secrets::MQTT_USER, Secrets::MQTT_PASS, "machine/connectstatus", 1, true, "offline"))
        {
            Serial.println("Connected.");
            mqttClient.publish("machine/connectstatus", "online", true);
            // subscribe to topic
            mqttClient.subscribe("test/topic");
            mqttClient.subscribe("machine/bottle");
            mqttClient.subscribe("machine/bottles");
        }
    }
} // Machine::connectMqtt()

void Machine::mqttCallback(char *topic, byte *payload, unsigned int length)
{
    String topicStr = String(topic);
    // TODO: handle incoming data
    String message = String((char *)payload, length);
    Serial.print("Callback - ");
    Serial.print("Message: ");
    Serial.println(message);
    Serial.print("Topic: ");
    Serial.println(topicStr);

    if (topicStr == "machine/bottle")
    {
        StaticJsonDocument<200> doc;
        deserializeJson(doc, payload, length);

        int id = doc["_id"].as<int>();

        if (id < 0 || id > MOTOR_COUNT)
        {
            throw("Invalid bottle id: " + String(id));
        }
        bottles[id].name = doc["name"].as<String>();
        bottles[id].active = doc["status"].as<bool>();
        bottles[id].costPerOz = doc["costPerOz"].as<float>();
        bottles[id].estimatedCapacity = doc["ozRemaining"].as<float>();
        bottles[id].totalCapacity = doc["ozCapacity"].as<float>();

        Serial.println("Bottle " + String(id) + " updated.");
        Serial.println(bottles[id].name);
        Serial.println(bottles[id].id);
        Serial.println(bottles[id].active);
        Serial.println(bottles[id].isShot);
        Serial.println(bottles[id].costPerOz);
        Serial.println(bottles[id].estimatedCapacity);
        Serial.println(bottles[id].totalCapacity);
        Serial.println();
        saveNVSBottle(id, bottles[id]);
    }
    else if (topicStr == "machine/bottles")
    {
    }
    else
    {
        std::vector<InputData> inputs = formatInputString(message);
        inputDecisionTree(inputs);
    }

} // Machine::mqttCallback()

void Machine::txMachineStatus()
{
    // if no wifi/mqtt connection, return
    if (!mqttClient.connected())
    {
        Serial.println("MQTT not connected, cannot send machine status.");
        return;
    } // if

    // bottles
    StaticJsonDocument<1024> bottlesJson;
    for (auto bottle : bottles)
    {
        JsonObject bottleObj = bottlesJson.createNestedObject();
        bottleObj["_id"] = bottle.id;
        bottleObj["name"] = bottle.name;
        bottleObj["ozCapacity"] = bottle.totalCapacity;
        bottleObj["ozRemaining"] = bottle.estimatedCapacity;
        bottleObj["status"] = bottle.active;
        bottleObj["costPerOz"] = bottle.costPerOz;

    } // for bottle
    String jsonOutput;
    serializeJson(bottlesJson, jsonOutput);
    Serial.println("Bottles JSON Bytes:" + String(jsonOutput.length()));
    Serial.println("Bottles JSON: ");
    Serial.println(jsonOutput.c_str());
    mqttClient.publish("machine/bottles", "Sending bottles data.");

    if (!mqttClient.publish("machine/bottles", jsonOutput.c_str()))
    {
        Serial.println("MQTT Publish Failed.");
        throw MqttFailedPublish("MQTT Publish Failed.");
    }
} // Machine::txMachineStatus()

void FrontLEDControl::clear()
{
    fill_solid(leds, FRONT_LED_COUNT, CRGB::Black);
    FastLED.show();
}; // FrontLEDControl::clear()

void FrontLEDControl::standby()
{
    int currentBrightness = FastLED.getBrightness();
    int targetBrightness = 50;
    int brightnessStep = (targetBrightness - currentBrightness) / 10;

    for (int i = 0; i < 10; i++)
    {
        FastLED.setBrightness(currentBrightness + brightnessStep * i);
        fill_solid(leds, FRONT_LED_COUNT, CRGB::Blue);
        FastLED.show();
        delay(10);
    }

}; // FrontLEDControl::standby()

void FrontLEDControl::updateProgress(double percentage)
{
    int ledsToLight = (percentage * FRONT_LED_COUNT) / 100;

    for (int i = lastUpdatedLed + 1; i < ledsToLight; i++)
    {
        leds[i] = CRGB::Red;
    }

    lastUpdatedLed = ledsToLight - 1;

    FastLED.show();

} // FrontLEDControl::updateProgress()

void FrontLEDControl::successAnimation()
{
    // 1. Green Chase on a Black Strip
    int chaseLength = 5; // Number of consecutive LEDs to light up in the chase
    int chaseRounds = 2; // Number of times the chase should go around the strip
    int chaseDelay = 8;  // Milliseconds delay for the chase effect (controls speed)

    for (int round = 0; round < chaseRounds; round++)
    {
        for (int i = 0; i < FRONT_LED_COUNT; i++)
        {
            clear(); // Ensure all LEDs are off to start with

            // Turn on the LEDs for the chase segment
            for (int j = 0; j < chaseLength && (i + j) < FRONT_LED_COUNT; j++)
            {
                leds[i + j] = CRGB::Red;
            }

            FastLED.show();
            delay(chaseDelay);
        }
    }

    // 2. Entire Strip Fade to Green
    int initialBrightness = 50;
    int finalBrightness = 255;
    int numFadeSteps = 30;
    int brightnessStep = (finalBrightness - initialBrightness) / numFadeSteps;
    int fadeDelay = 25; // Milliseconds delay for fade effect

    fill_solid(leds, FRONT_LED_COUNT, CRGB::Red); // Set all LEDs to green

    for (int brightness = initialBrightness; brightness <= finalBrightness; brightness += brightnessStep)
    {
        FastLED.setBrightness(brightness);
        FastLED.show();
        delay(fadeDelay);
    }

    FastLED.setBrightness(255); // Ensure brightness is fully restored
} // FrontLEDControl::successAnimation()

void FrontLEDControl::errorAnimation()
{
    // Flash red a few times
    int numFlashes = 3;
    int flashDelay = 200; // Milliseconds delay for each flash
    for (int i = 0; i < numFlashes; i++)
    {
        fill_solid(leds, FRONT_LED_COUNT, CRGB::Green);
        FastLED.show();
        delay(flashDelay);
        clear();
        FastLED.show();
        delay(flashDelay);
    }
    fill_solid(leds, FRONT_LED_COUNT, CRGB::Green);
    FastLED.show();

} // FrontLEDControl::errorAnimation()