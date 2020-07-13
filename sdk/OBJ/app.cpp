#include <cstdlib>
#include "ev3cxx.h"
#include "app.h"

using namespace ev3cxx;
using namespace std;

double integral = 0, lastError = 0;
double kp = 1;
double ki = 0.1;
double kd = 10;

void pid(int leftV, int rightV, int speed, Motor *mLeft, Motor *mRight)
{
    double error = (rightV - leftV);
	integral = (integral + error) / 2;
	double derivative = error - lastError;
	lastError = error;

	double result = kp * error + ki * integral + kd * derivative;

	int speedRight = (speed + (int)result);
	int speedLeft = (speed - (int)result);

	mRight->unregulated(speedRight);
	mLeft->unregulated(-speedLeft);
}

void main_task(intptr_t unused) 
{
    ColorSensor leftSensor(SensorPort::S3);
    ColorSensor rightSensor(SensorPort::S4);

    Motor leftMotor(MotorPort::B, MotorType::UNREGULATED);
    Motor rightMotor(MotorPort::C, MotorType::UNREGULATED);

    StopWatch sw;

    BrickButton enterBtn(BrickButtons::ENTER);
    enterBtn.waitForPress();

    sw.reset();
    int counter = 0;
    leftMotor.on(50);
    rightMotor.on(50);
    while(sw.getMs() < 60000)
    {
        rgb_raw_t left = leftSensor.reflectedRawRgb();
        rgb_raw_t right = rightSensor.reflectedRawRgb();
        counter++;
    }
    leftMotor.off();
    rightMotor.off();
    display.format("%  ")%counter;
    /*while(true)
    {
        display.format("left %  \nright %  \n\n\n")% leftSensor.reflectedRaw() % rightSensor.reflectedRaw();
    }*/
    
}
