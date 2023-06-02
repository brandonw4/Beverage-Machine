#include "Machine.hpp"

void Machine::boot()
{
    Serial.begin(115200);
    Serial.println("Serial 1 init on ");
    Serial2.begin(115200);
    Serial.println("Serial 2 init on 115200");

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
    } // try
    catch (String msg)
    {
        Serial.println(msg);
        touchscreen.controlCurPage("t3", "txt", msg);
    } // catch msg
    catch (std::exception &e)
    {
        touchscreen.controlCurPage("t3", "txt", "ERROR 100: GENERIC EXCEPTION THROWN, BOOT FAILED.");
        Serial.println(e.what());

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
        File curBottle = SD.open(path);
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
        bev.additionalInstructions = inst;
        beverages.push_back(bev);
    } // for i
    data.close();
} // Machine::initData()

String TouchControl::checkForInput()
{
    String touchInput = "";

    if (Serial2.available() > 0)
    {
        touchInput = Serial2.readStringUntil('@');
        // if (Serial2.readString() == "!") {
        //     touchInput = Serial2.readStringUntil('@');
        //     std::stringstream ss(touchInput.c_str());
        //     std::string cmd;
        //     while (ss >> cmd) {
        //         if (cmd == "gopage") {
        //             ss >> cmd;
        //             //
        //         } //if
        //     } //while
        // } //if

        Serial.println("Data from display: " + touchInput);
    } // while serial available

    return touchInput;
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
    int i = 0;
    for (auto beverage : beverages)
    {
        String itemId = "b" + String(i);
        i++;
        touchscreen.controlCurPage(itemId, "txt", beverage.name);
        if (beverage.isActive)
        {
            touchscreen.controlCurPage(itemId, "bco", "65088");
        } // if bev active
    }     // for each beverage
} // Machine::loadCocktailMenu()

void Machine::inputDecisionTree() {
    if (Serial2.available()) {
        String input = Serial2.readString();
        if (input.isEmpty()) {
            return;
        }
        Serial.println("Data from display: " + input);  // print the received data
        input.trim();  // remove any whitespace
        int separatorIndex = input.indexOf('@');

        // if we didn't find the '@' symbol, throw an error
        if (separatorIndex == -1) {
            Serial.println("Expected a '@' in the command, but received: " + input);
            return;
        }

        // split the command into two parts
        String command = input.substring(0, separatorIndex);
        command.trim();  // remove any whitespace

        String pageNumberStr = input.substring(separatorIndex + 1);
        pageNumberStr.trim();  // remove any whitespace

        // if the command is not '!gopage', throw an error
        if (command != "!gopage") {
            Serial.println("Received an invalid command: " + command);
            return;
        }

        // try to convert the page number to an integer
        int pageNumber = pageNumberStr.toInt();

        // if the page number is not between 0 and 99, throw an error
        if (pageNumber < 0 || pageNumber > 99) {
            Serial.println("Page number must be between 0 and 99, but received: " + pageNumberStr);
            return;
        }

        // if we got this far, everything is good, so print the page number
        Serial.print("Going to page ");
        Serial.println(pageNumber);

        switch(pageNumber) {
            case 1:
                loadMainMenu();
                break;
            case 4:
                loadAdminMenu();
                break;
            case 7:
                loadCocktailMenu();
                break;
            default:
                Serial.println("Invalid page number: " + pageNumber);
                break;
        } // switch(pageNumber

    }
} // Machine::inputDecisionTree()



void Beverage::createBeverage(std::vector<Bottle> &bottles)
{
    double bevTotalPrice = 0.0;

    if (!isActive)
    {
        Serial.println("Beverage " + name + " disabled.");
        // TODO: SEND TO TOUCH
        /*
        Beverage disabled, returning to menu.
        have a delay or something before returning
        */
        return;
    } // if !active

    for (size_t i = 0; i < MOTOR_COUNT; i++)
    {
        if (bottles[i].active == false && ozArr[i] > 0)
        {
            // TODO: throw a custom error, bottle out of stock
        } // if bottle is needed and inactive
    }     // for i

    // LoadCell.refreshDataSet(); //BETA Does this effect calibration?? //this was in there from old file

    // LoadCell.update(); TODO Loadcell
    if (/*LoadCell.getData() < 4*/ true)
    {
        /*
        TODO: no cup detected error
        */
    } // if no weight detected

} // Beverage::createBeverage()
