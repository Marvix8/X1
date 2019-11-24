#include "lib/Flex.h"
#include "lib/Hand.h"
#include "lib/Gesture.h"
#include "lib/MPU9250.h"
#include "lib/Kalman.h"
#include <SoftwareSerial.h>    // para la comunicación bluetooth

/*
   Asignación de sensores a los distintos pines del arduino.
*/
#define FLEX_SENSOR_PULGAR  A1
#define FLEX_SENSOR_INDICE  A0
#define FLEX_SENSOR_MEDIO   A2
#define BLUETOOTH_RX        7
#define BLUETOOTH_TX        8
#define BLUE_LED            4
#define RGB_LED_RED         9
#define RGB_LED_BLUE        6
#define RGB_LED_GREEN       5

/*
   Voltaje que reciven los sensores
*/
#define VCC 4.98 // Measured voltage of Ardunio 5V line

/*
 * Inputs recibidos desde bluetooth
 */
#define CALIBRATE_STRAIGHT_HAND 83
#define CALIBRATE_BEND_HAND 66
#define TURN_LED 76

#define DIVISOR_RESISTANCE_FLEX 100

// Valor a enviar vía bluetooth.
int mensajeBluetooth;

// Declarar objeto de comunicación bluetooth.
SoftwareSerial bluetooth(BLUETOOTH_RX, BLUETOOTH_TX);

// Declarar objetos de sensores
Flex bigFinger((double)DIVISOR_RESISTANCE_FLEX, 0.5);
Flex indexFinger((double)DIVISOR_RESISTANCE_FLEX, 0.8);
Flex middleFinger((double)DIVISOR_RESISTANCE_FLEX, 0.5);
// Objeto de acelerómetros
MPU9250 IMU(Wire,0x68);
Acelerometer acelerometer;

Hand hand(bigFinger, indexFinger, middleFinger, acelerometer);
Gesture gesture(hand);

// variables para verificar posición de mano
int previousPosition = -1;
int actualPosition = -1;
int counterSamePosition = 0;

// auxiliares para lectura de sensor flex
double bigFingerFlex;
double indexFingerFlex;
double middleFingerFlex;

// auxiliares para kalman
double filteredMeasurementX;
double filteredMeasurementY;
double filteredMeasurementZ;

// lectura de bluetooth
int bluetoothReader;
char bluetoothSender;

// Kalman para acelerometro
Kalman acelerometerXK(0.7, 15, 1023, 0);
Kalman acelerometerYK(0.8, 15, 1023, 0);
Kalman acelerometerZK(0.8, 8, 1023, 0);

// Prototipos de funciones
void sendInformationViaBluetooth (const int);
void rgbColor (int, int, int);
void ledRGB();

/*
   Configuración inicial de pines y comunicación.
*/
void setup()
{
  pinMode(FLEX_SENSOR_PULGAR, INPUT);
  pinMode(FLEX_SENSOR_INDICE, INPUT);
  pinMode(FLEX_SENSOR_MEDIO, INPUT);
  pinMode(BLUE_LED, OUTPUT);
  /*pinMode(RGB_LED_RED, OUTPUT);
  pinMode(RGB_LED_BLUE, OUTPUT);
  pinMode(RGB_LED_GREEN, OUTPUT);*/

  
  Serial.begin(9600); // Inicializa comunicación en serie.
  bluetooth.begin(9600); // Inicializa comunicación bluetooth.
  
  IMU.begin();
  IMU.setAccelRange(MPU9250::ACCEL_RANGE_8G);

  Serial.println("Setup OK");
}

/*
   Sentencias a ejecutar constantemente en el arduino.
*/
void loop()
{
  
  //Serial.println(Serial.read());
  if(bluetooth.available()){
    //bluetoothReader = Serial.read();
    bluetoothReader = bluetooth.read();

    if(bluetoothReader == CALIBRATE_BEND_HAND) {
      hand.calibrateBendHand((double)analogRead(FLEX_SENSOR_PULGAR), (double)analogRead(FLEX_SENSOR_INDICE), (double)analogRead(FLEX_SENSOR_MEDIO));
      Serial.print("P: ");
      Serial.println((double)analogRead(FLEX_SENSOR_PULGAR));
      Serial.print("I: ");
      Serial.println((double)analogRead(FLEX_SENSOR_INDICE));
      Serial.print("M: ");
      Serial.println((double)analogRead(FLEX_SENSOR_MEDIO));
      Serial.println("BendCal");
    }
  
    if(bluetoothReader == CALIBRATE_STRAIGHT_HAND){
      hand.calibrateStraightHand((double)analogRead(FLEX_SENSOR_PULGAR), (double)analogRead(FLEX_SENSOR_INDICE), (double)analogRead(FLEX_SENSOR_MEDIO));
      hand.setCalibrated(true);
      Serial.print("P: ");
      Serial.println((double)analogRead(FLEX_SENSOR_PULGAR));
      Serial.print("I: ");
      Serial.println((double)analogRead(FLEX_SENSOR_INDICE));
      Serial.print("M: ");
      Serial.println((double)analogRead(FLEX_SENSOR_MEDIO));
      Serial.println("StraightCal");
      Serial.println("Calibrated");
    }
    
    if(bluetoothReader == TURN_LED) {
      if(digitalRead(BLUE_LED) == LOW) {
        digitalWrite(BLUE_LED , HIGH);  
      } else {
        digitalWrite(BLUE_LED , LOW);
      }
    }
  }

  if(hand.getCalibrated() == true) {
      bigFingerFlex = (double)analogRead(FLEX_SENSOR_PULGAR);
      indexFingerFlex = (double)analogRead(FLEX_SENSOR_INDICE);
      middleFingerFlex = (double)analogRead(FLEX_SENSOR_MEDIO);

      hand.processPosition(bigFingerFlex, indexFingerFlex, middleFingerFlex);
      
      if (previousPosition == -1){
        previousPosition = hand.getHandPosition();
      } 
      actualPosition = hand.getHandPosition();
      
      if (actualPosition == previousPosition){
        counterSamePosition++;
      } else {
        counterSamePosition = 0;
      }
      previousPosition = actualPosition;
      
      IMU.readSensor();
      filteredMeasurementX = acelerometerXK.getFilteredValue(IMU.getAccelX_mss() * 0.1);
      filteredMeasurementY = acelerometerYK.getFilteredValue(IMU.getAccelY_mss() * 0.1);
      filteredMeasurementZ = acelerometerZK.getFilteredValue(IMU.getAccelZ_mss() * 0.1);
      
      if(counterSamePosition == 1500) {
        acelerometer.setAxisValues(filteredMeasurementX, filteredMeasurementY, filteredMeasurementZ);
        
        hand.process();
        gesture.readInput();
        gesture.updateStateMachine();

        Serial.print("P: ");
        Serial.print(bigFinger.getFlexPosition());
        Serial.print("\t");
        Serial.println(bigFingerFlex);
        Serial.print("I: ");
        Serial.print(indexFinger.getFlexPosition());
        Serial.print("\t");
        Serial.println(indexFingerFlex);
        Serial.print("M: ");
        Serial.print(middleFinger.getFlexPosition());
        Serial.print("\t");
        Serial.println(middleFingerFlex);
        
        switch(hand.getMovement()){
            case (int)SPZ:
              Serial.println("Star +Z");
              break;
            case (int)SNZ:
              Serial.println("Star -Z");
              break;
            case (int)SPY:
              Serial.println("Star +Y");
              break;
            case (int)SPX:
              Serial.println("Star +X");
              break;
            case (int)RPY:
              Serial.println("Rock +Y");
              break;
            case (int)RPZ:
              Serial.println("Rock +Z");
              break;
            case (int)GNZ:
              Serial.println("Good -Z");
              break;
            case (int)GPZ:
              Serial.println("Good +Z");
              break;
            case (int)GPX:
              Serial.println("Good +X");
              break;
            case (int)SCPY:
              Serial.println("Scissors +Y");
              break;
            case (int)SCNZ:
              Serial.println("Scissors -Z");
              break;
            case (int)UNDEFINED:
              Serial.println("Undefined");
              break;
          }
        //ledRGB();

        if(gesture.getAction() != '9' && gesture.getHasChanged() == true) {
          bluetoothSender = gesture.getAction();
          bluetooth.write(bluetoothSender);
          Serial.println("Acción a enviar: ");
          /*switch ((String)gesture.getAction()){
            case (String)PLAY:
              Serial.println("PLAY");
              break;
            case (String)PAUSE:
              Serial.println("PAUSE");
              break;
            case (String)STOP:
              Serial.println("STOP");
              break;
            case (String)PLUS_10S:
              Serial.println("PLUS_10S");
              break;
            case (String)LESS_10S:
              Serial.println("LESS_10S");
              break;
            case (String)PLUS_20S:
              Serial.println("PLUS_20S");
              break;
            }*/
          }
        gesture.setHasChanged(false);
        counterSamePosition = 0;
      }
    }
    else {
       //rgbColor(LOW, LOW, LOW);
    }
  
}



/*
 * Función utilizada para seleccionar el color a encender
 * del led RGB
 */
void ledRGB() {
  switch(hand.getMovement()){
    case (int)SPZ: rgbColor(255,0,0); break; // rojo
    case (int)SNZ: rgbColor(0,255,0); break; // verde
    case (int)SPY: rgbColor(0,0,255); break; // azul
    case (int)SPX: rgbColor(255,255,255); break; // blanco
    case (int)RPY: rgbColor(255,255,0); break; // amarillo
    case (int)RPZ: rgbColor(255,0,255); break; // magenta
    case (int)GNZ: rgbColor(0,255,255); break; // cyan
    case (int)GPZ: rgbColor(0,255,148); break; // violeta
    case (int)GPX: rgbColor(60,40,0); break; // naranja
    case (int)SCPY: rgbColor(128,0,0); break; // marrón
    case (int)SCNZ: rgbColor(0,0,128); break; // navy
    case (int)UNDEFINED: rgbColor(LOW,LOW,LOW); break; // apagado
    }
}


/*
 * Función utilizada para cambiar el color de led RGB
 * Entrada:
 * @redLightValue: cantidad de color rojo.
 * @blueLightValue: cantidad de color azul.
 * @greenLightValue: cantidad de color verde.
 */
void rgbColor (int redLightValue, int greenLightValue, int blueLightValue)
{
  analogWrite(RGB_LED_RED, redLightValue);
  analogWrite(RGB_LED_BLUE, blueLightValue);
  analogWrite(RGB_LED_GREEN, greenLightValue);
}
