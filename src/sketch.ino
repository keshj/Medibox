#include <Wire.h> // Wire library for I2C communication
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>

// OLED display width, height, and reset pin
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 //Reset pin #, or -1 if sharing Arduino reset pin
#define SCREEN_ADDRESS 0x3C

#define NTP_SERVER     "pool.ntp.org"  // NTP server for time synchronization
#define UTC_OFFSET_DST 0               // No daylight saving time in Colombo

// Pins
#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12

#define MIN_HEALTHY_TEMPERATURE 24 // Minimum temperature for healthy range
#define MAX_HEALTHY_TEMPERATURE 32 // Maximum temperature for healthy range
#define MIN_HEALTHY_HUMIDITY 65 // Minimum humidity for healthy range
#define MAX_HEALTHY_HUMIDITY 80 // Maximum humidity for healthy range

// Declare objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

// Declare global variables
int years = 0;
int months = 0;
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

int UTC_OFFSET = 19800; // Offset for Colombo: UTC+5:30 (5 * 3600 + 30 * 60 = 19800 seconds)
int UTC_OFFSET_HOURS = 5;
int UTC_OFFSET_MINUTES = 30;

unsigned long timeNow = 0;
unsigned long timeLast = 0;

bool alarm_enabled = true;
int n_alarms = 2;
int alarm_hours[] = {0, 1};
int alarm_minutes[] = {1, 10};
bool alarm_triggered[] = {false, false};

int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

int current_mode = 0;
int max_modes = 6;
String modes[] = {"1 - Set Time Zone", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Disable Alarm", "5 - View Active Alarms", "6 - Delete Alarm"};

void setup() {
  // put your setup code here, to run once:
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);

  dhtSensor.setup(DHTPIN, DHTesp::DHT22);

  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  display.display();
  delay(2000);

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WiFi", 0, 10, 2);
  }

  display.clearDisplay();
  print_line("Connected to WiFi", 0, 0, 2);
  delay(1000);

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  display.clearDisplay();

  print_line("Welcome to Medibox!", 0, 20, 2);
  delay(1000);
  display.clearDisplay();
}

void loop() {
  // put your main code here, to run repeatedly:
  update_time_with_check_alarm();
  if (digitalRead(PB_OK) == LOW) {
    delay(100);
    go_to_menu();
  }

  check_temp();
}

void print_line(String text, int column, int row, int textSize) {
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);

  display.display();
}


// Helper function to format integers with leading zeros
String format_with_zeros(int value, int digits) {
  String result = "";
  // Add leading zeros if needed
  for (int i = 0; i < digits - String(value).length(); i++) {
    result += "0";
  }
  result += String(value);
  return result;
}

void print_time_now() {
  display.clearDisplay();
  
  // Format date with leading zeros (YYYY:MM:DD)
  print_line(String(years), 0, 0, 1);
  print_line(":", 25, 0, 1);
  print_line(format_with_zeros(months, 2), 32, 0, 1);
  print_line(":", 45, 0, 1);
  print_line(format_with_zeros(days, 2), 52, 0, 1);
  
  // Format time with leading zeros (HH:MM:SS)
  print_line(format_with_zeros(hours, 2), 25, 20, 2);
  print_line(":", 45, 20, 2);
  print_line(format_with_zeros(minutes, 2), 55, 20, 2);
  print_line(":", 75, 20, 2);
  print_line(format_with_zeros(seconds, 2), 85, 20, 2);
}

void update_time() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  char timeHour[3];
  strftime(timeHour, 3, "%02H", &timeinfo);
  hours = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, 3, "%02M", &timeinfo);
  minutes = atoi(timeMinute);

  char timeSecond[3];
  strftime(timeSecond, 3, "%02S", &timeinfo);
  seconds = atoi(timeSecond);

  char timeDay[3];
  strftime(timeDay, 3, "%02d", &timeinfo);
  days = atoi(timeDay);

  char timeMonth[3];
  strftime(timeMonth, 3, "%02m", &timeinfo);
  months = atoi(timeMonth);

  char timeYear[5];  // Year requires 4 characters + null terminator
  strftime(timeYear, 5, "%04Y", &timeinfo);
  years = atoi(timeYear);
}


void ring_alarm() {
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2);
  digitalWrite(LED_1, HIGH);

  bool break_happened = false;

  // Ring the Buzzer
  while (digitalRead(PB_CANCEL) == HIGH && !break_happened) {
    for (int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);
        break_happened = true;
        break;
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  digitalWrite(LED_1, LOW);
  display.clearDisplay();
}

void update_time_with_check_alarm() {
  update_time();
  print_time_now();

  if (alarm_enabled) {
    for (int i = 0; i < n_alarms; i++) {
      if (!alarm_triggered[i] && hours == alarm_hours[i] && minutes == alarm_minutes[i]) {
        ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
}

int wait_for_button_press() {
  while (true) {
    if (digitalRead(PB_UP) == LOW) {
      delay(100);
      return PB_UP;
    }

    else if (digitalRead(PB_DOWN) == LOW) {
      delay(100);
      return PB_DOWN;
    }

    else if (digitalRead(PB_OK) == LOW) {
      delay(100);
      return PB_OK;
    }

    else if (digitalRead(PB_CANCEL) == LOW) {
      delay(100);
      return PB_CANCEL;
    }
  }
}

void go_to_menu() {
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(100);
      current_mode = (current_mode + 1) % max_modes;
    }

    else if (pressed == PB_DOWN) {
      delay(100);
      current_mode -= 1;
      if (current_mode < 0) {
        current_mode = max_modes - 1;
      }
    }

    else if (pressed == PB_OK) {
      delay(100);
      run_mode(current_mode);
    }

    else if (pressed == PB_CANCEL) {
      delay(100);
      break;
    }
  }
}

void set_time() {
  int temp_hour = hours;
  int temp_minute = minutes;

  while (true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(100);
      temp_hour = (temp_hour + 1) % 24;
    }

    else if (pressed == PB_DOWN) {
      delay(100);
      temp_hour -= 1;
      if (temp_hour < 0) {
        temp_hour = 23;
      }
    }

    else if (pressed == PB_OK) {
      delay(100);
      hours = temp_hour;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(100);
      break;
    }
  }

  while (true) {
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(100);
      temp_minute = (temp_minute + 1) % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(100);
      temp_minute -= 1;
      if (temp_minute < 0) {
        temp_minute = 59;
      }
    }

    else if (pressed == PB_OK) {
      delay(100);
      minutes = temp_minute;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(100);
      break;
    }
  }
  
  display.clearDisplay();
  print_line("Time set!", 0, 0, 2);
  delay(1000);
}

void set_time_zone() {
  int temp_hour = UTC_OFFSET_HOURS;
  int temp_minute = UTC_OFFSET_MINUTES;
  bool is_time_set = false;

  while (true) {
    display.clearDisplay();
    print_line("Enter UTC offset hours: " + String(temp_hour), 0, 0, 2);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(100);
      temp_hour = (temp_hour + 1) % 24;
    }

    else if (pressed == PB_DOWN) {
      delay(100);
      temp_hour -= 1;
      if (temp_hour < 0) {
        temp_hour = 23;
      }
    }

    else if (pressed == PB_OK) {
      delay(100);
      hours = temp_hour;
      is_time_set = true;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(100);
      break;
    }
  }

  while (true) {
    display.clearDisplay();
    print_line("Enter UTC offset minutes: " + String(temp_minute), 0, 0, 2);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(100);
      temp_minute = (temp_minute + 1) % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(100);
      temp_minute -= 1;
      if (temp_minute < 0) {
        temp_minute = 59;
      }
    }

    else if (pressed == PB_OK) {
      delay(100);
      minutes = temp_minute;
      is_time_set = true;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(100);
      break;
    }
  }

  UTC_OFFSET = UTC_OFFSET_HOURS * 3600 + UTC_OFFSET_MINUTES * 60;
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

  display.clearDisplay();
  if (is_time_set) {
    print_line("Time zone set!", 0, 0, 2);
    delay(1000);
  }

  else {
    print_line("Time zone not set!", 0, 0, 2);
    delay(1000);
  }
  
}

void set_alarm(int alarm) {
  int temp_hour = alarm_hours[alarm];
  int temp_minute = alarm_minutes[alarm];

  while (true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(100);
      temp_hour = (temp_hour + 1) % 24;
    }

    else if (pressed == PB_DOWN) {
      delay(100);
      temp_hour -= 1;
      if (temp_hour < 0) {
        temp_hour = 23;
      }
    }

    else if (pressed == PB_OK) {
      delay(100);
      alarm_hours[alarm] = temp_hour;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(100);
      break;
    }
  }

  while (true) {
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(100);
      temp_minute = (temp_minute + 1) % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(100);
      temp_minute -= 1;
      if (temp_minute < 0) {
        temp_minute = 59;
      }
    }

    else if (pressed == PB_OK) {
      delay(100);
      alarm_minutes[alarm] = temp_minute;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(100);
      break;
    }
  }
  
  display.clearDisplay();
  print_line("Alarm " + String(alarm + 1) + " set!", 0, 0, 2);
  delay(1000);
}

void run_mode(int mode) {
  if (mode == 0) {
    set_time_zone();
  }

  else if (mode == 1 || mode ==2) {
    set_alarm(mode - 1);
  }

  else if (mode == 3) {
    alarm_enabled = false;
  }

  else if (mode == 4) {
    view_active_alarms();
  }

  else if (mode == 5) {
    delete_alarm();
  }
}

void check_temp(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  if (data.temperature > MAX_HEALTHY_TEMPERATURE){
    //display.clearDisplay();
    print_line("TEMP HIGH", 0, 40, 1);
  }
  else if (data.temperature < MIN_HEALTHY_TEMPERATURE){
    //display.clearDisplay();
    print_line("TEMP LOW", 0, 40, 1);
  }
  if (data.humidity > MAX_HEALTHY_HUMIDITY){
    //display.clearDisplay();
    print_line("HUMIDITY HIGH", 0, 50, 1);
  }
  else if (data.humidity < MIN_HEALTHY_HUMIDITY){
    //display.clearDisplay();
    print_line("HUMIDITY LOW", 0, 50, 1);
  }
}

void view_active_alarms() {
    display.clearDisplay();
    print_line("Active Alarms", 0, 0, 2);
    delay(1000);

    for (int i = 0; i < n_alarms; i++) {
      if (alarm_enabled && alarm_triggered[i] == false) {
        print_line("Alarm " + String(i + 1) + " at " + String(alarm_hours[i]) + ":" + String(alarm_minutes[i]), 0, 20 + (i * 20), 1);
      }
      else if (alarm_triggered[i] == true) {
        print_line("Alarm " + String(i + 1) + " is already Triggered", 0, 20 + (i * 20), 1);
      }
      else if (!alarm_enabled && alarm_triggered[i] == false) {
        print_line("Alarm " + String(i + 1) + " is Disabled", 0, 20 + (i * 20), 1);
      }
    }
    delay(2000);
}

void delete_alarm(){
    int alarm_to_delete = -1;
    while (true) {
      display.clearDisplay();
      print_line("Enter Alarm to Delete: " + String(alarm_to_delete + 1), 0, 0, 2);
      int pressed = wait_for_button_press();
      if (pressed == PB_UP) {
        delay(100);
        alarm_to_delete += 1;
        alarm_to_delete = alarm_to_delete % n_alarms;
      }

      else if (pressed == PB_DOWN) {
        delay(100);
        alarm_to_delete -= 1;
        if (alarm_to_delete < -1) {
          alarm_to_delete = n_alarms - 1;
        }
      }

      else if (pressed == PB_OK) {
        delay(100);
        alarm_triggered[alarm_to_delete] = true;
        break;
      }

      else if (pressed == PB_CANCEL) {
        delay(100);
        break;
      }
    }

    display.clearDisplay();
    print_line("Alarm " + String(alarm_to_delete + 1) + " deleted!", 0, 0, 2);
    delay(2000);
}