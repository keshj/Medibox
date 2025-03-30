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
#define PB_SNOOZE 25
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
bool OFFSET_NEGATIVE = false; // Flag to indicate if the offset is negative

unsigned long timeNow = 0;
unsigned long timeLast = 0;

bool alarm_enabled = true;
#define n_alarms 2
int alarm_hours[n_alarms];
int alarm_minutes[n_alarms];
bool alarm_triggered[] = {false, false};
bool alarm_set[] = {false, false};

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
String modes[] = {"1 - Set Time Zone", "2 - Set Alarm 1", "3 - Set Alarm 2", "4 - Enable/Disable Alarms", "5 - View Active Alarms", "6 - Delete Alarm"};

void setup() {
  // put your setup code here, to run once:
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  pinMode(PB_SNOOZE, INPUT);

  dhtSensor.setup(DHTPIN, DHTesp::DHT22);

  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  display.display();
  delay(2000);

  // Connecting to the server
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


// Fetching the current time from the NTP server and displaying on OLED
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

void print_time_now() {
  display.clearDisplay();
  
  // Format date with leading zeros (YYYY:MM:DD)
  print_line(String(years), 0, 0, 1);
  print_line(":", 25, 0, 1);
  print_line(format_with_zeros(months, 2), 32, 0, 1);
  print_line(":", 45, 0, 1);
  print_line(format_with_zeros(days, 2), 52, 0, 1);
  
  // Format time with leading zeros (HH:MM:SS)
  print_line(format_with_zeros(hours, 2), 25, 15, 2);
  print_line(":", 45, 20, 2);
  print_line(format_with_zeros(minutes, 2), 55, 15, 2);
  print_line(":", 75, 20, 2);
  print_line(format_with_zeros(seconds, 2), 85, 15, 2);

  String signStr = OFFSET_NEGATIVE ? "-" : "+";
  print_line("UTC OFFSET: " + signStr + format_with_zeros(UTC_OFFSET_HOURS, 2) + ":" + format_with_zeros(UTC_OFFSET_MINUTES, 2), 0, 35, 1);
}


// Entering and navigating the menu
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
      current_mode = 0; // next time the user open the menu, it will start from the first mode
      delay(100);
      break;
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

void run_mode(int mode) {
  if (mode == 0) {
    set_time_zone();
  }

  else if (mode == 1 || mode ==2) {
    set_alarm(mode - 1);
  }

  else if (mode == 3) {
    alarm_enabled = !alarm_enabled;
  }

  else if (mode == 4) {
    view_active_alarms();
  }

  else if (mode == 5) {
    delete_alarm();
  }
}


// Setting time zone
void set_time_zone() {
  int utc_hour_offset = UTC_OFFSET_HOURS;
  int utc_minute_offset = UTC_OFFSET_MINUTES;
  bool is_time_set = false;
  bool offset_negative = OFFSET_NEGATIVE;      

  // Select offset sign
  while (true) {
    display.clearDisplay();
    String signStr = offset_negative ? "-" : "+";
    print_line("UTC offset sign: " + signStr, 0, 0, 2);
    print_line("UP/DOWN to toggle", 0, 40, 1);
    print_line("OK to confirm", 0, 50, 1);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP || pressed == PB_DOWN) {
      offset_negative = !offset_negative;
      delay(100);
    }
    else if (pressed == PB_OK) {
      delay(100);
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(100);
      break;
    }
  }

  while (true) {
    display.clearDisplay();
    print_line("Enter UTC offset hours: " + String(utc_hour_offset), 0, 0, 2);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(100);
      utc_hour_offset = (utc_hour_offset + 1) % 24;
    }

    else if (pressed == PB_DOWN) {
      delay(100);
      utc_hour_offset -= 1;
      if (utc_hour_offset < 0) {
        utc_hour_offset = 23;
      }
    }

    else if (pressed == PB_OK) {
      delay(100);
      //hours = utc_hour_offset;
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
    print_line("Enter UTC offset minutes: " + String(utc_minute_offset), 0, 0, 2);
    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      delay(100);
      utc_minute_offset = (utc_minute_offset + 1) % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(100);
      utc_minute_offset -= 1;
      if (utc_minute_offset < 0) {
        utc_minute_offset = 59;
      }
    }

    else if (pressed == PB_OK) {
      delay(100);
      //minutes = utc_minute_offset;
      is_time_set = true;
      break;
    }

    else if (pressed == PB_CANCEL) {
      delay(100);
      break;
    }
  }

  // Update the global variables with the new values
  UTC_OFFSET_HOURS = utc_hour_offset;
  UTC_OFFSET_MINUTES = utc_minute_offset;
  OFFSET_NEGATIVE = offset_negative;

  UTC_OFFSET = UTC_OFFSET_HOURS * 3600 + UTC_OFFSET_MINUTES * 60;
  if (OFFSET_NEGATIVE) {
    UTC_OFFSET = -UTC_OFFSET;
  }

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


// Setting and viewing alarms
void set_alarm(int alarm) {
  int temp_hour = alarm_hours[alarm];
  int temp_minute = alarm_minutes[alarm];
  alarm_triggered[alarm] = false;

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
      alarm_set[alarm] = true;
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
      alarm_set[alarm] = true;
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

void view_active_alarms() {
    display.clearDisplay();
    print_line("Viewing Alarms", 0, 0, 1);

    unsigned long startTime= millis();

    while (millis() - startTime < 10000 && digitalRead(PB_CANCEL) == HIGH) {
      for (int i = 0; i < n_alarms; i++) {
        if (!alarm_set[i]) {
          print_line("Alarm " + String(i + 1) + ": Not set", 0, 20 + (i * 20), 1);; // Skip if the alarm is not set
        }
        else if (alarm_enabled && alarm_triggered[i] == false) {
          print_line("Alarm " + String(i + 1) + " at " + format_with_zeros(alarm_hours[i], 2) + ":" + format_with_zeros(alarm_minutes[i], 2), 0, 20 + (i * 20), 1);
        }
        else if (alarm_triggered[i] == true) {
          print_line("Alarm " + String(i + 1) + " is already Triggered", 0, 20 + (i * 20), 1);
        }
        else if (!alarm_enabled && alarm_triggered[i] == false) {
          print_line("Alarm " + String(i + 1) + " at " + format_with_zeros(alarm_hours[i], 2) + ":" + format_with_zeros(alarm_minutes[i], 2) + " is Disabled", 0, 20 + (i * 20), 1);
        }
      }
    }
    current_mode = 0; // Reset to the first mode
}


// Ring an alarm, stopping and snoozing it
void update_time_with_check_alarm() {
  update_time();
  print_time_now();

  if (alarm_enabled) {
    for (int i = 0; i < n_alarms; i++) {
      if (alarm_set[i] && !alarm_triggered[i] && hours == alarm_hours[i] && minutes == alarm_minutes[i]) {
        ring_alarm(i);
      }
    }
  }
}

void ring_alarm(int alarm) {
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 20, 2);
  digitalWrite(LED_1, HIGH);

  bool break_happened = false;

  // Ring the Buzzer
  while (digitalRead(PB_CANCEL) == HIGH && !break_happened) {
    for (int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        delay(200);
        break_happened = true;
        alarm_triggered[alarm] = true;
        break;
      }
      if (digitalRead(PB_SNOOZE) == LOW) {
        delay(200);
        snooze_alarm(alarm);
        break_happened = true;
        alarm_triggered[alarm] = false;
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

void snooze_alarm(int alarm) {
    update_time();

    alarm_minutes[alarm] += 5;
    if (alarm_minutes[alarm] >= 60) {
      alarm_hours[alarm] += 1;
      alarm_minutes[alarm] = alarm_minutes[alarm] % 60;
      alarm_hours[alarm] = alarm_hours[alarm] % 24;
    }

    display.clearDisplay();
    print_line("Alarm snoozed for 5 minutes!", 0, 0, 2);
    delay(2000);
}


// Deleting a specific alarm
void delete_alarm(){
    int alarm_to_delete = 0;
    bool selectionMade = false;

    while (!selectionMade) {
      display.clearDisplay();
      print_line("Enter Alarm to Delete: " + String(alarm_to_delete + 1), 0, 0, 1);
      print_line("Press OK to delete", 0, 40, 1);
      print_line("or CANCEL to exit", 0, 50, 1);
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
        // Mark the alarm as not set (deleted)
        alarm_set[alarm_to_delete] = false;
        alarm_triggered[alarm_to_delete] = false;
        selectionMade = true;
      }

      else if (pressed == PB_CANCEL) {
        delay(100);
        return;
      }
    }

    display.clearDisplay();
    print_line("Alarm " + String(alarm_to_delete + 1) + " deleted!", 0, 0, 2);
    delay(2000);
}


// Temp and humidity warnings
void check_temp(){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  if (data.temperature > MAX_HEALTHY_TEMPERATURE){
    print_line("TEMP: HIGH", 0, 45, 1);
    digitalWrite(LED_1, HIGH);
  }
  else if (data.temperature < MIN_HEALTHY_TEMPERATURE){
    print_line("TEMP: LOW", 0, 45, 1);
    digitalWrite(LED_1, HIGH);
  }
  else {
    print_line("TEMP: OK", 0, 45, 1);
    digitalWrite(LED_1, LOW);
  }
  if (data.humidity > MAX_HEALTHY_HUMIDITY){
    print_line("HUMIDITY: HIGH", 0, 55, 1);
    digitalWrite(LED_1, HIGH);
  }
  else if (data.humidity < MIN_HEALTHY_HUMIDITY){
    print_line("HUMIDITY: LOW", 0, 55, 1);
    digitalWrite(LED_1, HIGH);
  }
  else {
    print_line("HUMIDITY: OK", 0, 55, 1);
    digitalWrite(LED_1, LOW);
  }
}
