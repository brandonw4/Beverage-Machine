#include "Machine.hpp"


void Machine::boot() {
    Serial.begin(115200);
    Serial.println("Serial 1 init on ");
    Serial2.begin(115200);
    Serial.println("Serial 2 init on 115200");

    touchscreen.controlCurPage("t3", "txt", "Serial 2 connected on 115200");



    //reserve and fill vectors
    bottles.reserve(MOTOR_COUNT);
    beverages.reserve(BEV_COUNT);
    //TODO: These need to be filled from SD Card data
    try {
        initSD();
        initConfig();
        initData();
    } //try
    catch(String msg) {
        Serial.println(msg);
        touchscreen.controlCurPage("t3","txt",msg);
    } //catch msg
    catch(std::exception& e) {
        touchscreen.controlCurPage("t3", "txt", "ERROR 100: GENERIC EXCEPTION THROWN, BOOT FAILED.");
        Serial.println(e.what());

    } //catch

    Serial.println("Boot successful."); 
    Serial.println("authCocktail: " + String(authCocktail) + " authShots: " + String(authShots));

    Serial.println("ONBOARD BOTTLE DATA READ: ");
    for (auto bottle:bottles) {
        Serial.println(bottle.name);
        Serial.println(bottle.id);
        Serial.println(bottle.active);
        Serial.println(bottle.isShot);
        Serial.println(bottle.costPerOz);
        Serial.println(bottle.estimatedCapacity);
        Serial.println();
    } //for every bottle

    touchscreen.changePage("1");
} //Machine::boot()

void Machine::initSD() {
    //BUG: Not handling errors. If there is an issue with .begin() it will just keep rebooting without info provided to user.
    if (!SD.begin(CS_PIN)) {
        throw ("ERROR 101: SD CARD FAILED INIT.");
    } //if SD init failed
    config = SD.open("/config.txt");
    if (!config) {
        throw("ERROR 102: config.txt FILE ERROR.");
    } //if config fail
    data = SD.open("/data.txt");
    if (!data) {
        throw("ERROR 102: data.txt FILE ERROR.");
    } //if log fail
    log = SD.open("/log.txt");
    if (!log) {
        throw("ERROR 102: log.txt FILE ERROR.");
    } //if log fail
    log.close();
} //Machine::initSD()

void Machine::initConfig() {
    //Read authCocktail
    String line = config.readStringUntil('\n');
    authCocktail = line.substring(line.indexOf('=') + 1).toInt();
    line = config.readStringUntil('\n');
    authShots = line.substring(line.indexOf('=') + 1).toInt();
    config.close();
} //Machine::initConfig()

void Machine::initData() {
    for (size_t i = 0; i < MOTOR_COUNT; i++) {
        Bottle bottle;
        bottle.name = data.readStringUntil('\n');
        bottle.id = data.readStringUntil('\n').toInt();
        int bottleIsActive = data.readStringUntil('\n').toInt();
        if (bottleIsActive != 1 && bottleIsActive != 0) {
            String error = "FILE ERROR Bottle Index: " + String(i) + " invalid type isActive bool.";
            throw(error);
        } //if invalid type
        int bottleIsShot = data.readStringUntil('\n').toInt();
        if (bottleIsShot != 1 && bottleIsShot != 0) {
            String error = "FILE ERROR Bottle Index: " + String(i) + " invalid type isShot bool.";
            throw(error);
        } //if invalid type
        bottle.active = bottleIsActive;
        bottle.isShot = bottleIsShot;
        bottle.costPerOz = data.readStringUntil('\n').toDouble();
        bottle.estimatedCapacity = data.readStringUntil('\n').toDouble();
        bottles.push_back(bottle);
    } //for i
    data.close();
} //Machine::initData()


String TouchControl::checkForInput() {
    String touchInput = "";
    
    if (Serial2.available() > 0) {
        touchInput = Serial2.readStringUntil('@');
        Serial.println("Data from display: " + touchInput);
    } //while serial available

    return touchInput;
}

void TouchControl::touchOutput(String str) {
    for (size_t i = 0; i < str.length(); i++) {
        Serial2.write(str[i]);
    } //for i
    
    flushStream();

} //TouchControl::touchOutput

void TouchControl::controlCurPage(String item, String cmd, String val) {
            flushStream();
            String send = "";
            send.concat(item);
            send.concat('.');
            send.concat(cmd);
            send.concat('=');
            if(cmd == "txt") {
                send.concat("\"");
                send.concat(val);
                send.concat("\"");
            } //if txt
            else {
                send.concat(val);
            } //else
            
            printDebug(send);
} //TouchControl::controlCurPage()

void Beverage::createBeverage(std::vector<Bottle> &bottles) {
    double bevTotalPrice = 0.0;

    if(!isActive) {
        Serial.println("Beverage " + name + " disabled.");
        //TODO: SEND TO TOUCH
        /*
        Beverage disabled, returning to menu.
        have a delay or something before returning
        */
       return;
    } //if !active

    for (size_t i = 0; i < MOTOR_COUNT; i++) {
        if (bottles[i].active == false && ozArr[i] > 0) {
            //TODO: throw a custom error, bottle out of stock
        } //if bottle is needed and inactive
    } //for i

    //LoadCell.refreshDataSet(); //BETA Does this effect calibration?? //this was in there from old file

    //LoadCell.update(); TODO Loadcell
    if (/*LoadCell.getData() < 4*/ true) {
        /*
        TODO: no cup detected error
        */
    } //if no weight detected



} //Beverage::createBeverage()
