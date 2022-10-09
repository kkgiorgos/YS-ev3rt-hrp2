#include <cstdlib>

#include <vector>
#include <queue>
#include <list>
#include <map>
#include <cstdio>
#include <climits>
#include <cmath>

#include "routes.h"
#include "tasks.h"
#include "methods.h"

#include "ev3ys.h"
#include "globalRobot.h"
#include "app.h"

using namespace ev3ys;
using namespace ev3cxx;
using namespace std;

//#define CALIBRATION 

Bluetooth bt;
FILE *bluetooth;

motor grabber(MotorPort::A, true);
motor ramp(MotorPort::D, false);
motor leftMotor(MotorPort::B, true, MotorType::MEDIUM);
motor rightMotor(MotorPort::C, false, MotorType::MEDIUM);
chassis robot(&leftMotor, &rightMotor, 6.24, 17, 0.01, 0.01, 0.1);
colorSensor leftSensor(SensorPort::S2, false, "WRO2022");
colorSensor rightSensor(SensorPort::S3, false, "WRO2022");
colorSensor leftScanner(SensorPort::S1, false, "WRO2022");
colorSensor rightScanner(SensorPort::S4, false, "WRO2022");
lineFollower lifo(400, &robot, &leftSensor, &rightSensor);
timer universalTimer;

queue<items, list<items>> rampQueue;
colors laundryBaskets[3];
map<colors, room> rooms;

double KP = 6;      //OLD: 0.5
double KI = 0.6;    //OLD: 0.05
double KD = 60;     //OLD: 5

double slowKP = 2.5;    //3
double slowKI = 1.5;    //1
double slowKD = 80;     //60

matPos startPos;
matPos currentPos;
orientation currentDirection;
lifoRobotPosition currentAlignment;

std::vector<int> graph[V];

BrickButton btnEnter(BrickButtons::ENTER);

bool grabberUsed = false;
bool startPicking = false;
bool stopScanning = false;
int scanStage = 0;

colorSensor *scanner;
colors scannedValue;

void startData()
{
    rooms.insert(pair<colors, room>(RED, room(RED)));
    rooms.insert(pair<colors, room>(GREEN, room(GREEN)));
    rooms.insert(pair<colors, room>(BLUE, room(BLUE)));
    rooms.insert(pair<colors, room>(YELLOW, room(YELLOW)));

    currentPos = S;
    currentDirection = NORTH;
}

void init()
{
    freopen("logOut.txt","w+", stderr);
    #ifdef DEBUG_BLUETOOTH
        bluetooth = ev3_serial_open_file(EV3_SERIAL_BT);
    #endif

    robot.setMode(speedMode::CONTROLLED);
    robot.setLinearAccelParams(100, 0, 0);
    robot.setAngularAccelParams(1000, 0, 0);
    robot.setStallTolerance(10, 200, 6, 40, 0.5);

    lifo.setDoubleFollowMode("SL", "SR");
    lifo.initializeMotionMode(speedMode::CONTROLLED);
    lifo.setSensorMode(sensorModes::REFLECTED);
    lifo.setAccelParams(100, 5, 50);
    lifo.addPIDparams(0, 4, 1, 30);
    lifo.forcePIDparams(false);

    leftSensor.setNormalisation(true);
    leftSensor.setFilteringRef(false);
    leftSensor.setFilteringRGB(false);
    leftSensor.setCutoffValue(27);
    rightSensor.setNormalisation(true);
    rightSensor.setFilteringRef(false);
    rightSensor.setFilteringRGB(false);
    rightSensor.setCutoffValue(27);

    leftScanner.setFilteringRef(false);
    leftScanner.setFilteringRGB(false);
    rightScanner.setFilteringRef(false);
    rightScanner.setFilteringRGB(false);
    leftScanner.setNormalisation(true);
    rightScanner.setNormalisation(true);
    
    grabber.setMode(REGULATED);
    ramp.setMode(REGULATED);
    //Initialize position of mechanisms (here or after start of mission, e.g. btn press)

    display.format("WAIT FOR SENSORS\n");
    btnEnter.waitForClick();
    act_tsk(INIT_TASK);
    tslp_tsk(1);
}

void open_grabber_task(intptr_t unused)
{
    //OPENS GRABBER FULLY
    grabber.setMode(REGULATED);
    grabber.moveUnlimited(-1200, true);
    tslp_tsk(50);
    while(grabber.getCurrentSpeed() > -900)
    {
        grabber.moveUnlimited(-1200);
        tslp_tsk(1);
    }
    while(grabber.getCurrentSpeed() < -800)
    {
        grabber.moveUnlimited(-1200);
        tslp_tsk(1);
    }
    grabber.stop(BRAKE);
}

void init_task(intptr_t unused)
{
    //INITIALIZATION OF RAMP AND GRABBER
    grabber.setMode(REGULATED);
    grabber.moveUnlimited(-1000, true);
    tslp_tsk(50);
    while(grabber.getCurrentSpeed() < -400)
    {
        grabber.moveUnlimited(-1000);
        tslp_tsk(1);
    }
    grabber.stop(BRAKE);
    ramp.setMode(REGULATED);
    ramp.moveUnlimited(-800, true);
    tslp_tsk(50);
    while(abs(ramp.getCurrentSpeed()) > 50)
    {
        ramp.moveUnlimited(-800);
        tslp_tsk(1);
    }
    ramp.stop(BRAKE);
}

void close_ramp_task(intptr_t unused)
{
    //CLOSES RAMP
    ramp.setMode(REGULATED);
    ramp.moveUnlimited(-1000, true);
    tslp_tsk(150);
    while(abs(ramp.getCurrentSpeed()) > 400)
    {
        ramp.moveUnlimited(-1000);
        tslp_tsk(1);
    }
    ramp.stop(BRAKE);
}

void water_grabber_task(intptr_t unused)
{
    //PICKS ONE OBJECT AND IMMEDIATELY OPENS GRABBER TO BE READY FOR THE NEXT
    grabberUsed = true;
    grabber.moveDegrees(1200, 350, NONE);
    grabber.moveUnlimited(700, true);
    while(grabber.getCurrentSpeed() > 600)
    {
        grabber.moveUnlimited(700);
        tslp_tsk(1);
    }
    grabber.stop(COAST);
    grabber.setMode(REGULATED);
    grabber.moveUnlimited(-1200, true);
    tslp_tsk(50);
    while(grabber.getCurrentSpeed() > -900)
    {
        grabber.moveUnlimited(-1200);
        tslp_tsk(1);
    }
    while(grabber.getCurrentSpeed() < -800)
    {
        grabber.moveUnlimited(-1200);
        tslp_tsk(1);
    }
    grabber.stop(BRAKE);
    grabberUsed = false;
}

void pick_block_task(intptr_t unused)
{
    //RAISES GRABBER FULLY
    grabber.moveDegrees(1200, 350, NONE);
    grabber.moveUnlimited(700, true);
    while(grabber.getCurrentSpeed() > 600)
    {
        grabber.moveUnlimited(700);
        tslp_tsk(1);
    }
    grabber.stop(COAST);
}

void empty_water_ramp_task(intptr_t unused)
{
    //LEAVES WATER ON TABLE FINAL STAGE
    emptyRampWaterStage2();
}

void basket_scan_task(intptr_t unused)
{
    //LOOKS FOR SIDE BASKETS
    colors leftBasket = BLACK;  //rightScanner used for left Basket
    colors rightBasket = BLACK; //leftScanner used for right Basket
    map<colors, int> appearancesLeft, appearancesRight;
    colors currentLeft, currentRight;
    bool notBlackLeft = false, notBlackRight = false;
    while(!stopScanning)
    {
        if((currentLeft = scanLaundryBasket(leftScanner)) != BLACK)
        {
            appearancesLeft[currentLeft]++;
            notBlackLeft = true;
        }
        if((currentRight = scanLaundryBasket(rightScanner)) != BLACK)
        {
            appearancesRight[currentRight]++;
            notBlackRight = true;
        }
        tslp_tsk(10);
    }
    if(notBlackLeft)
    {
        int maxCount = 0;
        for(auto x: appearancesLeft)
        {
            if(x.second > maxCount)
            {
                maxCount = x.second;
                rightBasket = x.first;    
            }
        }
    }
    if(notBlackRight)
    {
        int maxCount = 0;
        for(auto x: appearancesRight)
        {
            if(x.second > maxCount)
            {
                maxCount = x.second;
                leftBasket = x.first;    
            }
        }
    }

    laundryBaskets[BASKET_LEFT] = leftBasket;
    laundryBaskets[BASKET_RIGHT] = rightBasket;
}

void room_task_scan_task(intptr_t unused)
{
    colors current = NO_COLOR;
    map<colors, int> appearances;
    while(!stopScanning)
    {
        if((current = scanCodeBlock(*scanner)) != NO_COLOR)
            appearances[current]++;
        tslp_tsk(1);
    }
    current = analyzeFrequency(appearances, NO_COLOR);
    scannedValue = current;
}

void end_task(intptr_t unused)
{
    //CLOSES EVERYTHING
    grabber.setMode(REGULATED);
    grabber.moveSeconds(500, 1, BRAKE);
    ramp.setMode(REGULATED);
    ramp.moveUnlimited(-800, true);
    tslp_tsk(50);
    while(abs(ramp.getCurrentSpeed()) > 50)
    {
        ramp.moveUnlimited(-800);
        tslp_tsk(1);
    }
    ramp.stop(BRAKE);   
}

void main_task(intptr_t unused) 
{
    bt.open();
    format(bt, "\n\rSTARTING\n\r");
    format(bt, "Battery Voltage: %  \n\r")%ev3_battery_voltage_mV();
    init();
    timer missionTimer;

    timer t;

    vector<int> path;
    graphInit();
    startData();

    


    // leftSensor.setNormalisation(true);
    // rightSensor.setNormalisation(true);
    // display.resetScreen();
    // while(true)
    // {
    //     display.format("L: %  \nR: %  \n\n\n")%leftSensor.getReflected() %rightSensor.getReflected();
    //     // colorspaceRGB ll = leftScanner.getRGB();
    //     // colorspaceRGB rr = rightScanner.getRGB();
    //     // format(bt, "LL: R: %  G: %  B: %  W:   \nRR: R: %  G: %  B: %  W:   \n") %ll.red %ll.green %ll.blue %rr.red %rr.green %rr.blue;
    //     // rgb_raw_t l; ev3_color_sensor_get_rgb_raw(EV3_PORT_1, &l);
    //     // rgb_raw_t r; ev3_color_sensor_get_rgb_raw(EV3_PORT_4, &r);
    //     // format(bt, "LL: R: %  G: %  B: %  W:   \nRR: R: %  G: %  B: %  W:   \n") %l.r %l.g %l.b %r.r %r.g %r.b;
    //     // colorspaceHSV l2 = leftScanner.getHSV();
    //     // colorspaceHSV r2 = rightScanner.getHSV();
    //     // format(bt, "L: H: %  S: %  V: %  \nW: %  C: %  \n") %l2.hue %l2.saturation %l2.value %ll.white %static_cast<int>(leftScanner.getColor());
    //     // format(bt, "R: H: %  S: %  V: %  \nW: %  C: %  \n") %r2.hue %r2.saturation %r2.value %rr.white %static_cast<int>(rightScanner.getColor());
    //     // display.format("L: %  \nR: %  \n\n\n") %static_cast<int>(leftScanner.getColor()) %static_cast<int>(rightScanner.getColor());
    //     tslp_tsk(10);
    // }

    
    // while(true)
    // {
    //     grabber.setMode(REGULATED);
    //     grabber.moveUnlimited(-1200, true);
    //     tslp_tsk(50);
    //     while(grabber.getCurrentSpeed() > -900)
    //     {
    //         grabber.moveUnlimited(-1200);
    //         tslp_tsk(1);
    //     }
    //     while(grabber.getCurrentSpeed() < -800)
    //     {
    //         grabber.moveUnlimited(-1200);
    //         tslp_tsk(1);
    //     }
    //     grabber.stop(BRAKE);
    //     // btnEnter.waitForClick();
    //     // grabber.moveDegrees(800, 250, NONE, true);
    //     // grabber.moveDegrees(400, 100, BRAKE, false);
    //     // btnEnter.waitForClick();
    //     // grabber.setMode(REGULATED);
    //     // grabber.moveDegrees(400, 50, BRAKE, false);
    //     btnEnter.waitForClick(); 
    //     grabber.moveDegrees(1200, 350, NONE);
    //     grabber.moveUnlimited(700, true);
    //     while(grabber.getCurrentSpeed() > 600)
    //     {
    //         grabber.moveUnlimited(700);
    //         tslp_tsk(1);
    //     }
    //     grabber.stop(COAST);
    //     // btnEnter.waitForClick();
    // }

    // ev3_sensor_config(EV3_PORT_2, COLOR_SENSOR);
    // ev3_sensor_config(EV3_PORT_3, COLOR_SENSOR);
    // while(true)
    // {
    //     ref_raw_t l; ev3_color_sensor_get_raw_reflect(EV3_PORT_2, &l);
    //     ref_raw_t r; ev3_color_sensor_get_raw_reflect(EV3_PORT_3, &r);
    //     format(bt, "LL: L: %  B: %  \nRR: L: %  B: %  \n") %l.l %l.b %r.l %r.b;
    //     delayUs(60);
    // }


    leftSensor.setFilteringRef(true, 0.01, 10);
    leftSensor.setFilteringRGB(true, 0.01, 30);

    rightSensor.setFilteringRef(true, 0.01, 10);
    rightSensor.setFilteringRGB(true, 0.01, 30);


    lifo.setDoubleFollowMode("SL", "SR");
    lifo.initializeMotionMode(UNREGULATED);
    robot.setUnregulatedDPS(true);
    lifo.setSensorMode(REFLECTED);
    lifo.addPIDparams(30, 4, 1, 30);    //30 speed
    lifo.addPIDparams(40, 4, 3, 40);    //40 speed
    lifo.addPIDparams(50, 5, 4, 40);    //50 speed
    lifo.addPIDparams(60, 6, 5, 60);    //60 speed

    
    //Get out of start area
    robot.setMode(CONTROLLED);
    robot.setLinearAccelParams(100, 0, 40);
    robot.straight(60, 15, NONE);
    //S->W
    lifo.distance(40, 12, NONE);
    lifo.lines(40, 1, NONE);

    //Pick First Block
    robot.setLinearAccelParams(100, 30, 30);
    robot.straight(30, 5, NONE);
    act_tsk(WATER_GRABBER_TASK);
    tslp_tsk(1);
    robot.straight(30, 1, COAST);

    //Pick Second Block
    robot.setLinearAccelParams(100, 0, 0);
    robot.arc(60, -40, -8.5, COAST);
    robot.setLinearAccelParams(100, 0, 30);
    robot.straight(60, 10, NONE);
    act_tsk(PICK_BLOCK_TASK);
    tslp_tsk(1);
    robot.setLinearAccelParams(100, 30, 0);
    robot.straight(30, 3, COAST);

    //Get to TR intersection
    robot.setLinearAccelParams(100, 0, 0);
    robot.arc(60, 90, 3, COAST);

    lifo.setPIDparams(3, 3, 120);
    lifo.distance(30, 7, NONE);
    lifo.forcePIDparams(false);
    lifo.distance(40, 8, NONE);
    lifo.lines(40, 1, COAST);

    //Special turn to go from TR to CR2(nearly) and scan red room task
    robot.setMode(CONTROLLED);
    robot.setLinearAccelParams(100, 60, 60);
    robot.arc(60, 30, 15, NONE);
    robot.arc(60, 20, 40, NONE);
    
    stopScanning = false;
    scanner = &leftScanner;
    act_tsk(ROOM_TASK_SCAN_TASK);
    tslp_tsk(1);
    robot.arc(60, 52, 15, NONE);
    stopScanning = true;

    robot.setLinearAccelParams(100, 60, 60);
    robot.straight(60, 5, NONE);

    rooms[RED].setTask(scannedValue);
    display.resetScreen();
    display.format("%  \n")%static_cast<int>(scannedValue);
    
    //Straight move but uses lines for location help
    leftSensor.resetFiltering();
    robot.straightUnlim(60, true);
    do
    {
        leftSensor.getReflected();
        robot.straightUnlim(60);
    } while (!leftSensor.getLineDetected());
    robot.straight(60, 7, NONE);
    leftSensor.resetFiltering();
    robot.straightUnlim(60, true);
    do
    {
        leftSensor.getReflected();
        robot.straightUnlim(60);
    } while (!leftSensor.getLineDetected());
    robot.setLinearAccelParams(100, 60, 0);
    robot.straight(60, 9, COAST);

    //Turn wide back and limit with line
    robot.setLinearAccelParams(100, 0, -25);
    robot.arc(60, -85, 3.5, NONE);
    robot.setLinearAccelParams(100, -25, -25);
    robot.arcUnlim(25, 3.5, BACKWARD, true);
    while(leftSensor.getReflected() < 50 && abs(robot.getAngle()) < 10)
        robot.arcUnlim(25, 3.5, BACKWARD, false);
    robot.stop(COAST);

    //Lifo to CR3 intersection using blue-white lifo and scanning green room task
    lifo.setDoubleFollowMode("SL", "70");
    lifo.distance(30, 3, NONE);
    lifo.distance(40, 3, NONE);

    stopScanning = false;
    scanner = &rightScanner;
    act_tsk(ROOM_TASK_SCAN_TASK);
    tslp_tsk(1);
    robot.resetPosition();
    t.reset();
    lifo.lines(40, 2, NONE, 9, false);
    stopScanning = true;

    double speed = robot.getPosition() / t.secElapsed();
    robot.setLinearAccelParams(100, speed, 0);
    robot.straight(35, 8, COAST);

    rooms[GREEN].setTask(scannedValue);
    display.format("%  \n")%static_cast<int>(scannedValue);

    //Wide back turn limited with sensor
    robot.setLinearAccelParams(100, 0, -25);
    robot.arc(60, -85, -5, NONE);
    robot.setLinearAccelParams(100, -25, -25);
    robot.arcUnlim(25, -5, BACKWARD, true);
    while(leftSensor.getReflected() > 50 && abs(robot.getAngle()) < 10)
        robot.arcUnlim(25, -5, BACKWARD, false);
    robot.stop(COAST);

    //Room entrance lifo
    lifo.distance(30, 3, NONE);
    robot.resetPosition();
    t.reset();
    lifo.distance(40, 9, NONE);
    speed = robot.getPosition() / t.secElapsed();

    //Entering room
    robot.setLinearAccelParams(100, speed, 0);
    robot.straight(40, 30, BRAKE);

    btnEnter.waitForClick();


    resetLifo();
    lifo.setPIDparams(4, 2, 150);
    lifo.distance(robot.cmToTacho(30), 3, NONE);
    lifo.distance(robot.cmToTacho(40), 2, NONE);
    lifo.lines(robot.cmToTacho(60), 1, COAST);

    robot.setMode(CONTROLLED);
    robot.setLinearAccelParams(100, 60, 60);
    robot.arc(60, 30, 15, NONE);
    robot.arc(60, 20, 40, NONE);
    robot.arc(60, 52, 15, NONE);
    robot.setLinearAccelParams(100, 40, 60);
    robot.straightUnlim(60, true);
    robot.setLinearAccelParams(100, 60, 60);
    robot.straight(60, 5, NONE);
    leftSensor.resetFiltering();
    robot.straightUnlim(60, true);
    do{
        leftSensor.getReflected();
        robot.straightUnlim(60);
    }while(!leftSensor.getLineDetected());
    robot.straight(60, 7, NONE);
    leftSensor.resetFiltering();
    robot.straightUnlim(60, true);
    do{
        leftSensor.getReflected();
        robot.straightUnlim(60);
    }while(!leftSensor.getLineDetected());
    robot.setLinearAccelParams(150, 60, 0);
    robot.straight(60, 9, COAST);
    robot.setLinearAccelParams(100, 0, -25);
    robot.arc(60, -85, 3.5, NONE);
    robot.setLinearAccelParams(100, -25, -25);
    robot.arcUnlim(25, 3.5, BACKWARD, true);
    while(leftSensor.getReflected() < 50)
        robot.arcUnlim(25, 3.5, BACKWARD, false);
    robot.stop(COAST);

    resetLifo();

    lifo.setDoubleFollowMode("SL", "70");

    lifo.setPIDparams(4 * 3, 2 * 3, 150 * 3);
    lifo.distance(robot.cmToTacho(30), 3, NONE);
    lifo.distance(robot.cmToTacho(40), 2, NONE);
    lifo.lines(robot.cmToTacho(60), 1, NONE, 9, true);
    robot.resetPosition();
    t.reset();
    lifo.distance(robot.cmToTacho(40), 10, NONE);
    
    lifo.setPIDparams(2 * 3, 3 * 3, 150 * 3);
    lifo.initializeMotionMode(CONTROLLED);

    // lifo.setAccelParams(100, 50, 20);
    // lifo.distance(30, 7, NONE);
    speed = robot.getPosition()/t.secElapsed();
    lifo.setAccelParams(100, speed, 30);
    lifo.lines(30, 1, NONE);

    robot.setMode(CONTROLLED);  
    robot.setLinearAccelParams(150, 30, 0);
    robot.straight(30, 7, COAST);

    //Turn towards gree room
    robot.setLinearAccelParams(100, 0, -25);
    robot.arc(60, -85, -3.5, NONE);
    robot.setLinearAccelParams(100, -25, -25);
    robot.arcUnlim(25, -3.5, BACKWARD, true);
    while(leftSensor.getReflected() > 70)
        robot.arcUnlim(25, -3.5, BACKWARD);
    robot.stop(COAST);

    btnEnter.waitForClick();

    




    grabber.stop();
    // double min[4] = {1,2,1,2};
    // double max[4] = {68,73,55,196};
    // leftScanner.setRgbCalParams(min, max);
    leftSensor.setNormalisation(false);
    rightSensor.setNormalisation(false);
    leftScanner.setNormalisation(false);
    rightScanner.setNormalisation(false);
    display.resetScreen();
    while(true)
    {
        colorspaceRGB ll = leftScanner.getRGB();
        colorspaceRGB l = leftSensor.getRGB();
        colorspaceRGB r = rightSensor.getRGB();
        colorspaceRGB rr = rightScanner.getRGB();
        format(bt, "LL: R: %  G: %  B: %  W: %  \tL: R: %  G: %  B: %  W: %  \nR: R: %  G: %  B: %  W: %  \tRR: R: %  G: %  B: %  W: %  \n") %(int)ll.red %(int)ll.green %(int)ll.blue %(int)ll.white%(int)l.red %(int)l.green %(int)l.blue %(int)l.white %(int)r.red %(int)r.green %(int)r.blue %(int)r.white %(int)rr.red %(int)rr.green %(int)rr.blue %rr.white;
        // colorspaceHSV l2 = leftScanner.getHSV();
        // colorspaceHSV r2 = rightScanner.getHSV();
        // format(bt, "L: H: %  S: %  V: %  \nR: H: %  S: %  V: %  \n\n") %l2.hue %l2.saturation %l2.value %r2.hue %r2.saturation %r2.value;
        // display.format("L: %  \nR: %  \n\n\n") %static_cast<int>(scanLaundryBlock(leftScanner)) %static_cast<int>(scanLaundryBlock(rightScanner));
        tslp_tsk(10);
    }


    // btnEnter.waitForClick();

    // grabber.stop();
    // // resetLifo();
    // // lifo.setPIDparams(KP*1.2, KI * 0.7, KD*1.5);
    // // lifo.distance(robot.cmToTacho(30), 8, NONE);
    // // setLifoSlow();
    // // lifo.setAccelParams(150, 30, 30);
    // // lifo.distance(30, 6, NONE);
    // // lifo.lines(30, 1, NONE);
    // // lifo.distance(30, 1, NONE);
    // G_R(EAST);
    // rooms[BLUE].setTask(GREEN);
    // rooms[BLUE].executeAllActions();



    // startProcedure();

    // fullRouteStandard(W);

    // pickWater();

    // fullRouteStandard(G);
    // rooms[GREEN].executeAllActions();
    // fullRouteStandard(R);
    // rooms[RED].executeAllActions();
    // fullRouteStandard(B);
    // rooms[BLUE].executeAllActions();
    // fullRouteStandard(Y);
    // rooms[YELLOW].executeAllActions();
    // fullRouteStandard(L);

    // scanLaundryBaskets();
    // leaveLaundry();

    // fullRouteStandard(S);
    // finishProcedure();

    robot.stop(BRAKE);

    format(bt, "Mission Time: %  \r\n")%missionTimer.secElapsed();

    robot.stop(BRAKE);
    t.secDelay(1);
    format(bt, "\n\rENDING\n\r");
    bt.close();
}

//COLOR SENSOR TEST LOOPS
/*leftScanner.setNormalisation(false);
rightScanner.setNormalisation(true);
display.resetScreen();
while(true)
{
    colorspaceRGB left = leftScanner.getRGB();
    colorspaceRGB right = rightScanner.getRGB();
    tslp_tsk(10);
    display.format("L\nR:%  \nG:%  \nB:%  \nR\nR:%  \nG:%  \nB:%  \n")%left.red %left.green %left.blue %right.red %right.green %right.blue;
}*/
/*leftScanner.setNormalisation(false);
rightScanner.setNormalisation(false);
display.resetScreen();
while(true)
{
    colorspaceHSV left = leftScanner.getHSV();
    colorspaceHSV right = rightScanner.getHSV();
    tslp_tsk(10);
    display.format("L\nH:%  \nS:%  \nV:%  \nR\nH:%  \nS:%  \nV:%  \n")%left.hue %left.saturation %left.value %right.hue %right.saturation %right.value;
}*/
//leftSensor.setFiltering(false);
//leftSensor.setNormalisation(true);
//rightSensor.setFiltering(false);
//rightSensor.setNormalisation(true);
/*while(!btnEnter.isPressed())
{
    colorspaceRGB rgbL = leftSensor.getRGB();
    colorspaceRGB rgbR = rightSensor.getRGB();
    format(bt, "LS: R:%  G:%  B:%  W:%  \n\rRS: R:%  G:%  B:%  W:%  \n\n\n\r")%rgbL.red %rgbL.green %rgbL.blue %rgbL.white %rgbR.red %rgbR.green %rgbR.blue %rgbR.white;
    tslp_tsk(10);
}
t.secDelay(1);
while(!btnEnter.isPressed())
{
    format(bt, "LS: Ref: %  \n\rRS: Ref: %  \n\n\n\r")%leftSensor.getReflected() %rightSensor.getReflected();
    tslp_tsk(10);
}
t.secDelay(1);
while(!btnEnter.isPressed())
{
    colorspaceHSV hsvL = leftSensor.getHSV();
    colorspaceHSV hsvR = rightSensor.getHSV();
    format(bt, "LS: H:%  S:%  V:%  \n\rRS: H:%  S:%  V:%  \n\n\n\r")%hsvL.hue %hsvL.saturation %hsvL.value %hsvR.hue %hsvR.saturation %hsvR.value;
    tslp_tsk(10);
}
t.secDelay(1);
while(!btnEnter.isPressed())
{
    format(bt, "LS: col:%  \n\rRS: col:%  \n\n\n\r")%static_cast<int>(leftSensor.getColor()) %static_cast<int>(rightSensor.getColor());
    tslp_tsk(10);
}*/



//SIMPLE CALIBRATION RGB

/*leftSensor.setNormalisation(false);
rightSensor.setNormalisation(false);
double minLeft[4] = {1000, 1000, 1000, 1000};
double maxLeft[4] = {0, 0, 0, 0};
double minRight[4] = {1000, 1000, 1000, 1000};
double maxRight[4] = {0, 0, 0, 0};
t.secDelay(1);
btnEnter.waitForPress();
minLeft[0] = leftSensor.getRGB().red;
minLeft[1] = leftSensor.getRGB().green;
minLeft[2] = leftSensor.getRGB().blue;
minLeft[3] = leftSensor.getRGB().white;
t.secDelay(1);
btnEnter.waitForPress();
maxLeft[0] = leftSensor.getRGB().red;
maxLeft[1] = leftSensor.getRGB().green;
maxLeft[2] = leftSensor.getRGB().blue;
maxLeft[3] = leftSensor.getRGB().white;
t.secDelay(1);
btnEnter.waitForPress();
minRight[0] = rightSensor.getRGB().red;
minRight[1] = rightSensor.getRGB().green;
minRight[2] = rightSensor.getRGB().blue;
minRight[3] = rightSensor.getRGB().white;
t.secDelay(1);
btnEnter.waitForPress();
maxRight[0] = rightSensor.getRGB().red;
maxRight[1] = rightSensor.getRGB().green;
maxRight[2] = rightSensor.getRGB().blue;
maxRight[3] = rightSensor.getRGB().white;

leftSensor.setRgbCalParams(minLeft, maxLeft);
rightSensor.setRgbCalParams(minRight, maxRight);*/

//SIMPLE CALIBRATION REF
// leftSensor.setNormalisation(false);
// rightSensor.setNormalisation(false);
// double minLeft = 1000;
// double minRight = 1000;
// double maxLeft = 0;
// double maxRight = 0;
// t.secDelay(1);
// display.resetScreen();
// display.format("LEFT -> BLACK");
// btnEnter.waitForPress();
// minLeft = leftSensor.getReflected();
// t.secDelay(1);
// display.resetScreen();
// display.format("LEFT -> WHITE");
// btnEnter.waitForPress();
// maxLeft = leftSensor.getReflected();
// t.secDelay(1);
// display.resetScreen();
// display.format("RIGHT -> BLACK");
// btnEnter.waitForPress();
// minRight = rightSensor.getReflected();
// t.secDelay(1);
// display.resetScreen();
// display.format("RIGHT -> WHITE");
// btnEnter.waitForPress();
// maxRight = rightSensor.getReflected();
// leftSensor.setRefCalParams(minLeft, maxLeft);
// rightSensor.setRefCalParams(minRight, maxRight);
// btnEnter.waitForClick();

//GREATER LINE SENSOR CALIBRATION
    // leftSensor.setNormalisation(false);
    // rightSensor.setNormalisation(false);
    // double minLeft[4] = {1000, 1000, 1000, 1000};
    // double maxLeft[4] = {0, 0, 0, 0};
    // double minRight[4] = {1000, 1000, 1000, 1000};
    // double maxRight[4] = {0, 0, 0, 0};
    // int minLeftRef = 100;
    // int maxLeftRef = 0;
    // int minRightRef = 100;
    // int maxRightRef = 0;
    //
    // robot.setMode(CONTROLLED);
    // robot.setLinearAccelParams(100, 20, 20);
    // robot.straightUnlim(20, true);
    // t.reset();
    //
    // while(t.secElapsed() < 2)
    // {
    //     rgb_raw_t leftRGB; ev3_color_sensor_get_rgb_raw(EV3_PORT_2, &leftRGB);
    //     rgb_raw_t rightRGB; ev3_color_sensor_get_rgb_raw(EV3_PORT_3, &rightRGB);
    //
    //     minLeft[0] = min(minLeft[0], (double)leftRGB.r);
    //     minLeft[1] = min(minLeft[1], (double)leftRGB.g);
    //     minLeft[2] = min(minLeft[2], (double)leftRGB.b);
    //     minLeft[3] = min(minLeft[3], (double)leftRGB.r+leftRGB.g+leftRGB.b);
    //
    //     minRight[0] = min(minRight[0], (double)rightRGB.r);
    //     minRight[1] = min(minRight[1], (double)rightRGB.g);
    //     minRight[2] = min(minRight[2], (double)rightRGB.b);
    //     minRight[3] = min(minRight[3], (double)rightRGB.r+rightRGB.g+rightRGB.b);
    //
    //     maxLeft[0] = max(maxLeft[0], (double)leftRGB.r);
    //     maxLeft[1] = max(maxLeft[1], (double)leftRGB.g);
    //     maxLeft[2] = max(maxLeft[2], (double)leftRGB.b);
    //     maxLeft[3] = max(maxLeft[3], (double)leftRGB.r+leftRGB.g+leftRGB.b);
    //
    //     maxRight[0] = max(maxRight[0], (double)rightRGB.r);
    //     maxRight[1] = max(maxRight[1], (double)rightRGB.g);
    //     maxRight[2] = max(maxRight[2], (double)rightRGB.b);
    //     maxRight[3] = max(maxRight[3], (double)rightRGB.r+rightRGB.g+rightRGB.b);
    // }
    //
    // robot.stop(BRAKE);
    //
    // btnEnter.waitForPress();
    //
    // t.reset();
    // robot.straightUnlim(20, true);
    // while(t.secElapsed() < 2)
    // {
    //     int left = leftSensor.getReflected();
    //     int right = rightSensor.getReflected();
    //
    //     minLeftRef = min(minLeftRef, left);
    //     minRightRef = min(minRightRef, right);
    //
    //     maxLeftRef = max(maxLeftRef, left);
    //     maxRightRef = max(maxRightRef, right);
    // }
    //
    // robot.stop(BRAKE);
    //
    // leftSensor.setRefCalParams(minLeftRef, maxLeftRef);
    // rightSensor.setRefCalParams(minRightRef, maxRightRef);
    // leftSensor.setRgbCalParams(minLeft, maxLeft);
    // rightSensor.setRgbCalParams(minRight, maxRight);

// //SCANNER CALIBRATION 
    // //Min measurements done with BLACK block 4 STUDS away
    // //Max measurements done with WHITE block 0.5 STUDS away
    // double minL[4] = {0,0,0,0};
    // double maxL[4] = {0,0,0,0};
    // double minR[4] = {0,0,0,0};
    // double maxR[4] = {0,0,0,0};
    //
    // leftScanner.setNormalisation(false);
    // rightScanner.setNormalisation(false);
    // display.resetScreen();
    // BrickButton right(BrickButtons::RIGHT);
    // t.secDelay(1);
    // colorspaceRGB sum = {0, 0, 0, 0};
    // int times = 0;
    // display.format("Calibrating Left Scanner Min");
    // while(!btnEnter.isPressed())
    // {
    //     while(!right.isPressed() && !btnEnter.isPressed());
    //     rgb_raw_t l; ev3_color_sensor_get_rgb_raw(EV3_PORT_1, &l);
    //     format(bt, "LL: R: %  G: %  B: %  W:   \n") %l.r %l.g %l.b;
    //     sum.red += l.r;
    //     sum.green += l.g;
    //     sum.blue += l.b;
    //     times++;
    //     while(right.isPressed());
    // }
    // minL[0] = sum.red / (double)times;
    // minL[1] = sum.green / (double)times;
    // minL[2] = sum.blue / (double)times;
    // minL[3] = (sum.red+sum.green+sum.blue) / (double)times;
    // t.secDelay(1);
    // sum.red = sum.green = sum.blue = times = 0;
    // display.resetScreen();
    // display.format("Calibrating Left Scanner Max");
    // while(!btnEnter.isPressed())
    // {
    //     while(!right.isPressed() && !btnEnter.isPressed());
    //     rgb_raw_t l; ev3_color_sensor_get_rgb_raw(EV3_PORT_1, &l);
    //     format(bt, "LL: R: %  G: %  B: %  W:   \n") %l.r %l.g %l.b;
    //     sum.red += l.r;
    //     sum.green += l.g;
    //     sum.blue += l.b;
    //     times++;
    //     while(right.isPressed());
    // }
    // maxL[0] = sum.red / (double)times;
    // maxL[1] = sum.green / (double)times;
    // maxL[2] = sum.blue / (double)times;
    // maxL[3] = (sum.red+sum.green+sum.blue) / (double)times;
    // leftScanner.setRgbCalParams(minL, maxL);
    // leftScanner.setNormalisation(true);
    // display.resetScreen();
    // display.format("Calibration Left Scanner DONE");
    // format(bt, "LL: R: %  G: %  B: %  W:   \nRR: R: %  G: %  B: %  W:   \n") %minL[0] %minL[1] %minL[2] %maxL[0] %maxL[1] %maxL[2];
    //
    //
    // t.secDelay(1);
    // sum.red = sum.green = sum.blue = times = 0;
    // display.format("Calibrating Right Scanner Min");
    // while(!btnEnter.isPressed())
    // {
    //     while(!right.isPressed() && !btnEnter.isPressed());
    //     rgb_raw_t r; ev3_color_sensor_get_rgb_raw(EV3_PORT_4, &r);
    //     format(bt, "LL: R: %  G: %  B: %  W:   \n") %r.r %r.g %r.b;
    //     sum.red += r.r;
    //     sum.green += r.g;
    //     sum.blue += r.b;
    //     times++;
    //     while(right.isPressed());
    // }
    // minR[0] = sum.red / (double)times;
    // minR[1] = sum.green / (double)times;
    // minR[2] = sum.blue / (double)times;
    // minR[3] = (sum.red+sum.green+sum.blue) / (double)times;
    // t.secDelay(1);
    // sum.red = sum.green = sum.blue = times = 0;
    // display.resetScreen();
    // display.format("Calibrating Right Scanner Max");
    // while(!btnEnter.isPressed())
    // {
    //     while(!right.isPressed() && !btnEnter.isPressed());
    //     rgb_raw_t r; ev3_color_sensor_get_rgb_raw(EV3_PORT_4, &r);
    //     format(bt, "LL: R: %  G: %  B: %  W:   \n") %r.r %r.g %r.b;
    //     sum.red += r.r;
    //     sum.green += r.g;
    //     sum.blue += r.b;
    //     times++;
    //     while(right.isPressed());
    // }
    // maxR[0] = sum.red / (double)times;
    // maxR[1] = sum.green / (double)times;
    // maxR[2] = sum.blue / (double)times;
    // maxR[3] = (sum.red+sum.green+sum.blue) / (double)times;
    // rightScanner.setRgbCalParams(minR, maxR);
    // rightScanner.setNormalisation(true);
    // display.resetScreen();
    // display.format("Calibration Right Scanner DONE");
    // format(bt, "minL: R: %  G: %  B: %  W:   \nmaxL: R: %  G: %  B: %  W:   \n") %minL[0] %minL[1] %minL[2] %maxL[0] %maxL[1] %maxL[2];
    // format(bt, "minR: R: %  G: %  B: %  W:   \nmaxR: R: %  G: %  B: %  W:   \n") %minR[0] %minR[1] %minR[2] %maxR[0] %maxR[1] %maxR[2];
    //
    // color_hue hues[5] = {{RED, 10, 20}, {RED, 350, 20}, {GREEN, 140, 80}, 
    //                     {BLUE, 225, 50}, {YELLOW, 35, 30}};
    //
    // leftScanner.setColorCalParams(hues, 5, 70, 15);
    // rightScanner.setColorCalParams(hues, 5, 70, 15);

//Motor benchmark
    // ev3_motor_config(EV3_PORT_B, MEDIUM_MOTOR);
    // ev3_motor_config(EV3_PORT_C, MEDIUM_MOTOR);
    //
    // // FILE *log = fopen("WRO2022/batteryTest.txt", "w");
    // FILE *log = bluetooth;
    //
    // int setPower = 50;
    // int maxPower = 85;
    // int minPower = 40;
    // bool isPowerAsc = true;
    // int leftReportedPower, rightReportedPower;
    // double leftActualSpeed, rightActualSpeed;
    // int voltage = 8000, current, power;
    // while(voltage > 7000)
    // {
    //     ev3_motor_set_power(EV3_PORT_B, -setPower);
    //     ev3_motor_set_power(EV3_PORT_C, setPower);
    //     ev3_motor_reset_counts(EV3_PORT_B);
    //     ev3_motor_reset_counts(EV3_PORT_C);
    //     t.reset();
    //     t.secDelay(0.01);
    //     leftReportedPower = ev3_motor_get_power(EV3_PORT_B);
    //     rightReportedPower = ev3_motor_get_power(EV3_PORT_C);
    //     leftActualSpeed = ev3_motor_get_counts(EV3_PORT_B) / t.secElapsed();
    //     rightActualSpeed = ev3_motor_get_counts(EV3_PORT_C) / t.secElapsed();
    //     voltage = ev3_battery_voltage_mV();
    //     current = ev3_battery_current_mA();
    //     power = voltage * current;
    //
    //
    //     if(setPower == 0)
    //     {
    //         ev3_motor_stop(EV3_PORT_B, false);
    //         ev3_motor_stop(EV3_PORT_C, false);
    //
    //         t.secDelay(5);
    //
    //         voltage = ev3_battery_voltage_mV();
    //         current = ev3_battery_current_mA();
    //         power = voltage * current;
    //         fprintf(log, "%d\t%d\t%d\t%lf\t%lf\t%d\t%d\t%d\n", 0, 0, 0, 0.0, 0.0, voltage, current, power);
    //     }
    //     else
    //     {
    //         fprintf(log, "%d\t%d\t%d\t%lf\t%lf\t%d\t%d\t%d\n", setPower, leftReportedPower, rightReportedPower, leftActualSpeed, rightActualSpeed, voltage, current, power);
    //     }
    //  
    //     if(abs(setPower) < minPower)
    //         setPower = -sign(setPower) * minPower;
    //
    //     if(isPowerAsc && setPower < maxPower)
    //         setPower++;
    //     else if(!isPowerAsc && setPower > -maxPower)
    //         setPower--;
    //     else if(setPower == maxPower)
    //     {
    //         setPower--;
    //         isPowerAsc = false;
    //     }
    //     else
    //     {
    //         setPower++;
    //         isPowerAsc = true;
    //     }
    // }

/*//Calibration Debug
vector<char*> ports = {"S1", "S2", "S3", "S4"};
    for(auto port : ports)
    {
        tslp_tsk(1);
        double offsetRef, scaleRef;
        bool normalisedRef = true;
        char filename[32];
        sprintf(filename, "%s/%srefCal.txt", "WRO2022", port);
        FILE *calData = fopen(filename, "r+");
        if(calData != nullptr)
        {
            fscanf(calData, "%lf", &offsetRef);
            fscanf(calData, "%lf", &scaleRef);
        }
        else
            normalisedRef = false;
        fclose(calData);
        
        
        if(normalisedRef)
        {
            DEBUGPRINT("%s: REF:\tOffset:%lf\tScale:%lf\n", port, offsetRef, scaleRef);
        }


        tslp_tsk(1);
        double offsetRGB[4] = {0, 0, 0, 0}, scaleRGB[4] = {0, 0, 0, 0};
        bool normalisedRGB = true;
        sprintf(filename, "%s/%srgbCal.txt", "WRO2022", port);
        calData = fopen(filename, "r+");
        if(calData != nullptr)
        {
            for (int i = 0; i < 4; i++)
            {
                fscanf(calData, "%lf", &offsetRGB[i]);
                fscanf(calData, "%lf", &scaleRGB[i]);
            }
        }
        else
            normalisedRGB = false;
        
        fclose(calData);

        if(normalisedRGB)
        {
            DEBUGPRINT("%s: RGB:\nR:\tOffset:%lf\tScale:%lf\nG:\tOffset:%lf\tScale:%lf\nB:\tOffset:%lf\tScale:%lf\nW:\tOffset:%lf\tScale:%lf\n", port, offsetRGB[0], scaleRGB[0], offsetRGB[1], scaleRGB[1], offsetRGB[2], scaleRGB[2], offsetRGB[3], scaleRGB[3]);
        }

        tslp_tsk(1);
        sprintf(filename, "%s/%scolorCal.txt", "WRO2022", port);
        calData = fopen(filename, "r+");
        int colorsAmount = 5;
        colorCalibration colorData;
        colorData.hues.clear();
        if(calData != nullptr)
        {
            fscanf(calData, "%lf", &colorData.minColorSaturation);
            fscanf(calData, "%lf", &colorData.greyscaleIntersection);
            for (int i = 0; i < colorsAmount; i++)
            {
                color_hue data;
                fscanf(calData, "%d", &data.color);
                fscanf(calData, "%lf", &data.hue);
                fscanf(calData, "%lf", &data.zoneSize);
                colorData.hues.push_back(data);
            }
        }

        if(calData != nullptr)
        {
            DEBUGPRINT("%s: Color:\n", port);
            DEBUGPRINT("MinColorSaturation: %lf\n", colorData.minColorSaturation);
            DEBUGPRINT("GreyscaleIntersection: %lf\n", colorData.greyscaleIntersection);
            for (int i = 0; i < colorsAmount; i++)
            {
                DEBUGPRINT("Color: %d\t", colorData.hues[i].color);
                DEBUGPRINT("Mid Hue: %lf\t", colorData.hues[i].hue);
                DEBUGPRINT("Hue Range Size: %lf\n", colorData.hues[i].zoneSize);
            }
        }

        fclose(calData);

    }
    */
