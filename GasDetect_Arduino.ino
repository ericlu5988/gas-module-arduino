#include <LiquidCrystal_I2C.h>

// initialize lcd screen with 0x27 starting address, number of columns, and number of rows
LiquidCrystal_I2C lcd(0x27, 16, 2);

int RL_VALUE = 5;     //define the load resistance on the board, in kilo ohms
int RO_CLEAN_AIR_FACTOR = 9.83;
int CALIBRATION_SAMPLE_TIMES = 30;
int CALIBRATION_SAMPLE_INTERVAL = 500;
int READ_SAMPLE_INTERVAL = 50;
int READ_SAMPLE_TIMES = 5;

// initiate gas data curves (x, y, and slope) based off of datasheet chart
float LPGCurve[3]  =  {2.3, 0.21, -0.47};
float COCurve[3]  =  {2.3, 0.72, -0.34};
float SmokeCurve[3] = {2.3, 0.53, -0.44};
float Ro = 10;

// initialize gas values in values array
int GAS_LPG = 0;
int GAS_CO = 1;
int GAS_SMOKE = 2;

// initialize values for lpg, co, and smoke
float lpg = 0;
float co = 0;
float smoke = 0;

// initialize methods for reading values
float MQ2Read();
float MQGetGasPercentage(float rs_ro_ratio, int gas_id);
int MQGetPercentage(float rs_ro_ratio, float *pcurve);
float MQCalibration(int);
float MQResistanceCalculation(int raw_adc);

// last reading time
int lastReadTime = 0;

// assign LEDs and buzzer to pins on the UNO
int greenLed = 2;
int redLed = 3;
int buzzer = 12;

// assign buttons to pins on the UNO and set button states
int buttonExit = 5;
int buttonNext = 6;
int buttonEnter = 7;
int buttonStateExit = 0;
int buttonStateNext = 0;
int buttonStateEnter = 0;

// assign analog pin for sensor
int analogInput = A0;
int _pin = analogInput;

//float lpg, co, smoke;
float lpg_ppm, co_ppm, smoke_ppm;

// establish lpg/co/smoke threshold for testing
float lpgThresh = 2000;
float coThresh = 100;
float smokeThresh = 200;

// initialize serial comms and pins
void setup() {
  Serial.begin(9600);
  pinMode(redLed, OUTPUT);
  pinMode(greenLed, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(buttonExit, INPUT_PULLUP);
  pinMode(buttonNext, INPUT_PULLUP);
  pinMode(buttonEnter, INPUT_PULLUP);

  initDisplay();
}

void initDisplay()
{
  // set up the LCD display and display welcome message
  lcd.init(); // initialize the LCD
  lcd.backlight(); // turn on the backlight
  lcd.clear(); // clear the LCD
  lcd.setCursor(0, 0);
  lcd.print("Welcome. Press");
  lcd.setCursor(0, 1);
  lcd.print("ENT to Begin");

  // display welcome message for testing
  Serial.println("Select Gas Module to run using the button and LCD screen");
  Serial.print("Button Layout:");
  Serial.println(" Exit | Next Item | Enter");
}

// run loop to run program
void loop() {
  // read button values
  readButtons();

  // if enter button is pressed, then continue to base menu (MQ2)
  if (buttonStateEnter == LOW)
  {
    lcd.clear();

    // display menu
    display_MQ2Menu();
  }
}

void display_MQ2Menu()
{
  // display MQ2 base menu
  // press enter to begin reading MQ2 sensor
  // press next to select MQ1 sensor
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MQ2. ENT to BEG,");
  lcd.setCursor(0, 1);
  lcd.print("NXT for MQ1");

  // wait for user response
  delay(1000);

  // check for user response (next or enter)
  while (buttonStateEnter == HIGH || buttonStateNext == HIGH)
  {
    // read button values
    readButtons();

    //if enter then begin reading MQ2
    if (buttonStateEnter == LOW)
    {
      lcd.clear();
      lcd.print("Running MQ2");
      MQ2_Sensor();
    }
    // if next then show MQ1 menu
    else if (buttonStateNext == LOW)
    {
      display_MQ1Menu();
    }
  }
}

// function that displays the MQ1 menu
void display_MQ1Menu()
{
  // display MQ2 base menu
  // press enter to begin reading MQ1 sensor
  // press next to select MQ2 sensor
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MQ1. ENT to BEG,");
  lcd.setCursor(0, 1);
  lcd.print("NXT for MQ2");

  // wait for user response
  delay(1000);

  // check for user response (next or enter)
  while (buttonStateEnter == HIGH || buttonStateNext == HIGH)
  {
    // read button values
    readButtons();

    // if enter then begin reading MQ1
    if (buttonStateEnter == LOW)
    {
      lcd.clear();
      lcd.print("Running MQ1");
      delay(2000);
      MQ1_Sensor();
    }
    // if next then show MQ2 menu
    else if (buttonStateNext == LOW)
    {
      display_MQ2Menu();
    }
  }
}

// function that reads the MQ1 sensor
void MQ1_Sensor()
{
  lcd.clear();
  while (buttonStateExit == HIGH)
  {
    readButtons();
    if (buttonStateExit == LOW)
    {
      lcd.clear();
      lcd.print("Exiting...");
      stopReading();
      Serial.print("Exit was pressed\n\n");
      delay(2000);
      display_MQ1Menu();
    }
    // continue to read the values until user wants to exit with the exit button
    else
    {
      lcd.clear();
      lcd.print("MQ1 coming soon");
      delay(1000);
    }
  }
}

// function that reads the MQ2 sensor
void MQ2_Sensor() {

  // begin calibration of the sensor to grab clean air values
  begin_MQ2calibration();

  // keep running the loop until the exit button is pressed and held
  while (buttonStateExit == HIGH)
  {
    // read button values
    readButtons();

    // if exit button is pressed and held, stop reading the sensor and exit to the main MQ2 menu
    if (buttonStateExit == LOW)
    {
      lcd.clear();
      lcd.print("Exiting...");
      stopReading();
      Serial.print("Exit was pressed\n\n");
      delay(2000);
      display_MQ2Menu();
    }
    // continue to read the values until user wants to exit with the exit button
    else
    {
      // print the values through serial monitor and to the lcd screen
      // if you do not want to print the values, then set it to false
      float* values = read(true);

      // read individual values on the sensor
      lpg = readLPG();
      co = readCO();
      smoke = readSmoke();

      // if any of the values are over the given threshold, then audible and visible alarm will be triggered via buzzer and red LED
      if ((lpg > lpgThresh) || (co > coThresh) || (smoke > smokeThresh))
      {
        // smoke detected, red LED lit, green LED dark, buzzer on
        digitalWrite(redLed, HIGH);
        digitalWrite(greenLed, LOW);
        digitalWrite(buzzer, HIGH);
      }
      // gas not detected, green LED lit, red LED dark, buzzer off
      else
      {
        digitalWrite(redLed, LOW);
        digitalWrite(greenLed, HIGH);
        digitalWrite(buzzer, LOW);
      }
      Serial.println("");

      // delay .1 seconds before next reading
      delay(100);
    }
  }
}

// function to read button values (exit, next, enter)
void readButtons()
{
  buttonStateExit = digitalRead(buttonExit);
  buttonStateNext = digitalRead(buttonNext);
  buttonStateEnter = digitalRead(buttonEnter);
}

// function to shut off visual and audible alarms
void stopReading()
{
  digitalWrite(redLed, LOW);
  digitalWrite(greenLed, LOW);
  digitalWrite(buzzer, LOW);
}

/*######################################################################*/
/*
   1. initialize MQ2 device and get RO (sensor resistance in fresh air) value in kohms, RS (resistance in displayed gases at various concentrations)
   2. RS is the resistance of the sensor that changes depending on the concentration of gas, and R0 is the resistance of the sensor at a known concentration without the presence of other gases, or in fresh air.
   3. We can derive a formula to find RS using Ohm's Law: V = I x R
*/

void begin_MQ2calibration() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrating... ");

  Ro = MQ2Calibration();
  Serial.print("Ro: ");
  Serial.print(Ro);
  Serial.println(" kohm");
}

float MQ2Calibration() {
  float val = 0;

  for (int i = 0; i < CALIBRATION_SAMPLE_TIMES; i++) {      //take multiple samples
    val += MQResistanceCalculation(analogRead(_pin));
    delay(CALIBRATION_SAMPLE_INTERVAL);
  }
  val = val / CALIBRATION_SAMPLE_TIMES;                 //calculate the average value

  val = val / RO_CLEAN_AIR_FACTOR;                      //divided by RO_CLEAN_AIR_FACTOR yields the Ro
  //according to the chart in the datasheet
  return val;
}

// read analog values of the gas module
float* read(bool print) {

  lpg = MQGetGasPercentage(MQ2Read() / Ro, GAS_LPG);
  co = MQGetGasPercentage(MQ2Read() / Ro, GAS_CO);
  smoke = MQGetGasPercentage(MQ2Read() / Ro, GAS_SMOKE);

  // if boolean is true, print the values over serial monitor
  if (print) {
    lcd.clear();
    Serial.print("LPG:");
    Serial.print(lpg);
    Serial.print( "ppm" );
    Serial.print("    ");
    Serial.print("CO:");
    Serial.print(co);
    Serial.print( "ppm" );
    Serial.print("    ");
    Serial.print("SMOKE:");
    Serial.print(smoke);
    Serial.print( "ppm" );
    Serial.print("\n");
    lcd.setCursor(0, 0);
    lcd.print("LPG:");
    lcd.print(lpg);
    lcd.print(" CO:");
    lcd.print(co);
    lcd.setCursor(0, 1);
    lcd.print("SM:");
    lcd.print(smoke);
  }
  lastReadTime = millis();

  // return values of all gases
  static float values[3] = {lpg, co, smoke};

  return values;
}

float MQ2Read() {
  int i;
  float rs = 0;
  int val = analogRead(_pin);

  for (i = 0; i < READ_SAMPLE_TIMES; i++) {
    rs += MQResistanceCalculation(val);
    delay(READ_SAMPLE_INTERVAL);
  }

  rs = rs / READ_SAMPLE_TIMES;
  return rs;
}

float readLPG() {
  if (millis() < (lastReadTime + 10000) && lpg != 0) {
    return lpg;
  } else {
    return lpg = MQGetGasPercentage(MQ2Read() / 10, GAS_LPG);
  }
}

float readCO() {
  if (millis() < (lastReadTime + 10000) && co != 0) {
    return co;
  } else {
    return co = MQGetGasPercentage(MQ2Read() / 10, GAS_CO);
  }
}

float readSmoke() {
  if (millis() < (lastReadTime + 10000) && smoke != 0) {
    return smoke;
  } else {
    return smoke = MQGetGasPercentage(MQ2Read() / 10, GAS_SMOKE);
  }
}

float MQResistanceCalculation(int raw_adc) {
  return (((float)RL_VALUE * (1023 - raw_adc) / raw_adc));
}

float MQGetGasPercentage(float rs_ro_ratio, int gas_id) {
  if ( gas_id == GAS_LPG ) {
    return MQGetPercentage(rs_ro_ratio, LPGCurve);
  } else if ( gas_id == GAS_CO ) {
    return MQGetPercentage(rs_ro_ratio, COCurve);
  } else if ( gas_id == GAS_SMOKE ) {
    return MQGetPercentage(rs_ro_ratio, SmokeCurve);
  }
  return 0;
}

int MQGetPercentage(float rs_ro_ratio, float * pcurve) {
  return (pow(10, (((log(rs_ro_ratio) - pcurve[1]) / pcurve[2]) + pcurve[0])));
}
