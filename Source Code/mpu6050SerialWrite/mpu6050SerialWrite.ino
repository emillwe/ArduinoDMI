/**  mpu6050SerialWrite.c
 *   @file      mpu6050SerialWrite
 *   @author    Emmett Miller
 *   @author    Justin Schwiesow
 *   @date      14-August-2022
 *   @brief     Lab 4 Custom Project
 *   
 *  This is a demonstration of how to read from the gy-521 accelerometer/gyroscope 
 *  module and write these values to the serial bus and an LCD. While this
 *  application was written to be used with SuperCollider for audio processing,
 *  it could also be a control source for other software that can parse serial
 *  data.
 *  
 *  MPU6050 code appropriated from:
 *  I2C device class (I2Cdev) demonstration Arduino sketch for MPU6050 class
 *  10/7/2011 by Jeff Rowberg <jeff@rowberg.net>
 *  
 *  LCD code appropriated from Arduino LiquidCrystal documentation
 *  
 *  Initial idea to use Bluetooth controller via arduinio inspired by
 *  Dr. Marcin Pączkowski, UW DXARTS
 *  https://github.com/dyfer/arduino-wifi-supercollider-sensors
 *  
 *  Interfacing SuperCollider and Arduino via serial inspired 
 *  by Eli Fieldsteel's tutorials, University of Illinois.
 *  https://www.youtube.com/watch?v=_NpivsEva5o
 *  eli@illinois.edu
 */

#include <LiquidCrystal.h> // for 12x2 LCD display

#include <MPU6050.h> // for gy-521 module
#include <I2Cdev.h> // for gy-521 module and LCD

#include <Arduino_FreeRTOS.h> // operating system

/*
  connections:
  MEGA |   Gy-521
  VCC       VCC
  GND       GND
  20        SDA
  21        SCL
*/

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include "Wire.h"
#endif

#define LED_PIN 12      // pin 12 outputs to external LED 

#define valueScaler 2048.0  // normalize accelerometer data to factor of g = 9.8 m/s^2
#define accMax 15.75        // accelerometer normalization to to parameter range [-1, 1] 

// FreeRTOS task handles
TaskHandle_t t1, t2;

// class default I2C address is 0x68
// specific I2C addresses may be passed as a parameter here
// AD0 low = 0x68 (default for InvenSense evaluation board)
// AD0 high = 0x69
MPU6050 accelgyro;
//MPU6050 accelgyro(0x69); // <-- use for AD0 high

int16_t ax, ay, az; // accelerometer values
int16_t gx, gy, gz; // gyroscope values

// initialize the library by associating any needed LCD interface pin
// with the Arduino pin number it is connected to
const int rs = 42, en = 41, d4 = 44, d5 = 45, d6 = 46, d7 = 47; // TODO: Update pins
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

void setup() {
    //-----accelerometer setup-----
    // join I2C bus (I2Cdev library doesn't do this automatically)
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif

    // initialize serial communication
    Serial.begin(115200);
    
    // LED setup
    pinMode(12, OUTPUT);
  
    // wait for serial port to connect
    // Needed for native USB port only
    // this means that the sketch won't start without USB connected and Serial Monitor started
    while (!Serial) {
        ; // do nothing
    }

    // initialize device
    accelgyro.initialize();
    
    // verify connection
    while (!accelgyro.testConnection()) {
        // reinitializing doesn't work, so we'll stop here
        Serial.println("connection failed");
        delay(1000);
    }

    //-----LCD Setup-----
    // initialize for 16x2 display
    lcd.begin(16, 2);

    //-----FreeRTOS setup-----
    // create tasks
    xTaskCreate(
        rt1,
        "Flash LED",
        64,  // Stack size
        NULL,
        4,    // Priority
        &t1
    );
 
    xTaskCreate(
        readAcc,
        "Read/write Accelerometer",
        128,  // Stack size
        NULL,
        3,    // Priority
        &t1
    );
    
    xTaskCreate(
        printLCD,
        "Print to LCD",
        256,  // Stack size
        NULL,
        2,    // Priority
        &t2
    );
}

void loop() {

}

/**
 * @brief flash external LED ON for 100ms, OFF 200ms
 * 
 * <tt> rt1() </tt> flashes an external LED, connected to pin
 * <tt> LED_PIN </tt>.
 * 
 * @return returns void
 */
 void rt1() {
    while(1) {
        digitalWrite(LED_PIN, LOW);             // LED ON
        vTaskDelay(100 / portTICK_PERIOD_MS);
        digitalWrite(LED_PIN, HIGH);            // LED OFF
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief read raw x, y, z accelerometer values from gy-521
 * and print to serial monitor
 * 
 * <tt> readAcc() </tt> reads the raw data from the
 * gy-521 module. x, y, and z correspond to roll,
 * pitch, and net acceleration, respectively. The
 * function writes these values to the serial monitor
 * sequentially, each followed by a character, 'x',
 * 'y', or 'z' to designate its origin to whatever
 * software may be parsing the serial data.
 * 
 * @return returns void
 */
void readAcc() {
    while (1) {
        // read raw accel measurements from device    
        accelgyro.getAcceleration(&ax, &ay, &az);
    
        // write values to serial bus
        Serial.print(ax); Serial.print('x');
        Serial.print(ay); Serial.print("y");
        Serial.print(az); Serial.print("z");
    
        vTaskDelay(1);
    }
}

/**
 * @brief display normalized acceleration data on LCD
 * 
 * <tt> printLCD() </tt> writes the normalized
 * accelerometer values from the gy-521 module to a
 * 16x2 LCD. Normalized values correspond to the
 * parameter space of our SuperCollider processing
 * (scrub and reverb wet/dry) and much more
 * useful to see than raw data.
 * 
 * @return returns void
 */
void printLCD() {
    while (1) {
        // start at first row
        lcd.setCursor(0, 0);
        
        // print accelerometer values for sample scrub parameter
        lcd.print("SCRUB: "); 
        
        lcd.setCursor(7, 0);
        lcd.print(ax / valueScaler / accMax);
        
        // move to second row
        lcd.setCursor(0, 1);

        // print accelerometer values for reverb wet/dry parameter
        lcd.print("REVERB: "); lcd.print(ay / valueScaler / accMax);

        vTaskDelay(7);
    }
}
