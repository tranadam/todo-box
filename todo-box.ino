#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // set the LCD address to 0x27 for 16 chars and 2 line display

// ----------------------------------------------
// STRUCTURES -----------------------------------
// ----------------------------------------------

struct Button {
    unsigned int pin;
    bool isPressed;
};

struct Switch {
    unsigned int pin;
    int lastState;
};

struct Todo {
    unsigned int index;
    String name;
    unsigned int ledPin;
    Button btn;
    bool isDone;
};

struct RGLed {
    unsigned int redPin;
    unsigned int greenPin;
};

// ----------------------------------------------
// CONFIG ---------------------------------------
// ----------------------------------------------

Todo todos[] = {
        {0, "NO PHONE MORNING", A4,  {7, false}, false},
        {1, "20 MINUTES SPORT", A5,  {8, false}, false},
        {2, "JOURNAL 3 THINGS", 4,  {9, false}, false},
        {3, "XXXXXXXXXXXXXXXX", 5,  {10, false}, false},
        {4, "NO PHONE IN BED",  6,  {11, false}, false},
};

constexpr RGLed mainLed = {13, 12};
Switch saveSwitch = {A0, 0};
constexpr unsigned int dayCounterIndices[] = {0, 1, 2, 3};
constexpr unsigned int firstDayIndex = 4;

// ----------------------------------------------
// UTILITY FUNCTIONS ----------------------------
// ----------------------------------------------

/**
 * Resets bytes in EEPROM memory, where day counting is stored
 * must be called in a separate script to nullify these values
 * and then the main script can be uploaded
 */
void resetEEPROM() {
    for (unsigned int counterIndex: dayCounterIndices) {
        EEPROM.update(counterIndex, 0b00000000);
    }
}

/**
 * Increments the correct day-counting byte by one
 */
void incrementDayIndex() {
    for (unsigned int counterIndex: dayCounterIndices) {
        byte value;
        EEPROM.get(counterIndex, value);
        if (value != 0b11111111) {
            EEPROM.update(counterIndex, ++value);
            return;
        }
    }
}

/**
 * Gets the index of the latest day which was written into
 */
unsigned int getDayIndex() {
    unsigned int totalDays = 0;
    for (unsigned int counterIndex: dayCounterIndices) {
        byte days;
        EEPROM.get(counterIndex, days);
        totalDays += days;
    }
    return totalDays + firstDayIndex;
}

/**
 * Detects if the passed switch's position changed compared to the previous state
 */
bool switchChanged(Switch& sw) {
    if (int state = digitalRead(sw.pin); state != sw.lastState) {
        sw.lastState = state;
        return true;
    }
    return false;
}

/**
 * Detects if the passed button is pressed
 * Returns true only once even for long presses
 */
bool buttonPressed(Button& btn) {
    if (digitalRead(btn.pin) == LOW && !btn.isPressed) {
        btn.isPressed = true;
        return true;
    }
    btn.isPressed = digitalRead(btn.pin) == LOW;
    return false;
}

/**
 * Shows button click confirmation message
 * and displays the name of just changed task
*/
void showClickMsg(const Todo& todo) {
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print(todo.name);
    for (int i = 0; i < 16; ++i) {
        lcd.setCursor(todo.isDone ? i : 15 - i, 1);
        lcd.print(todo.isDone ? '+' : '-');
        delay(140);
    }
    delay(200);
    lcd.clear();
    lcd.noBacklight();
}

/**
 * Checks if all todos are done, returns true if so
 */
bool allTodosDone() {
    for (const Todo& item: todos) {
        if (!item.isDone) {
            return false;
        }
    }
    return true;
}

/**
 * Sets main led to green if all todos are done, otherwise sets to red
 */
void setMainLed() {
    digitalWrite(mainLed.redPin, allTodosDone() ? LOW : HIGH);
    digitalWrite(mainLed.greenPin, allTodosDone() ? HIGH : LOW);
}

/**
 * Returns a byte signalizing which tasks were done
 * First to fifth least significant bits represent each task
 */
byte todoDayResults() {
    unsigned int result = 0b0;
    for (const Todo& item: todos) {
        if (item.isDone) {
            result |= 0b00000001 << item.index;
        }
    }
    return result;
}

/**
 * Counts current streak for the mask passed
 * Returns the number of days in which at least the tasks
 * passed through the mask were done
 */
unsigned int countCurrentStreak(byte todoMask) {
    unsigned int latestDay = getDayIndex() - 1;
    unsigned int streak = 0;
    for (unsigned int dayIndex = latestDay; dayIndex >= firstDayIndex; --dayIndex) {
        byte value;
        EEPROM.get(dayIndex, value);
        if ((value & todoMask) ^ todoMask) {
            break;
        }
        ++streak;
    }
    return streak;
}

/**
 * Clears display in a cool way :o
 * Animates pipe as if it erased all text from left to right
 */
void clearDisplay() {
    for (int i = 15; i >= 0; --i) {
        delay(150);
        lcd.setCursor(i, 0);
        lcd.print(" ");
        lcd.setCursor(i, 1);
        lcd.print(" ");
    }
}

/**
 * Shows all information about the current stats and streaks
 */
void showStats() {
    lcd.setCursor(1, 0);
    lcd.print("CURRENT STREAK");
    lcd.setCursor(7, 1);
    lcd.print(countCurrentStreak(0b00011111));

    delay(3000);
    clearDisplay();

    for (const Todo& item: todos) {
        lcd.setCursor(0, 0);
        lcd.print(item.name);
        lcd.setCursor(0, 1);
        lcd.print("STREAK: ");
        lcd.print(countCurrentStreak(0b00000001 << item.index));
        delay(1000);
        clearDisplay();
    }
}

/**
 * Shows confirmation of save and information about current day and day result
 */
void showSaveValidation() {
    lcd.setCursor(0, 0);
    lcd.print("DAY: ");
    lcd.print(getDayIndex() - firstDayIndex);
    lcd.setCursor(0, 1);
    lcd.print("RESULT: ");
    for (const Todo& item: todos) {
        byte flagBit = 0b00000001 << item.index;
        lcd.print((todoDayResults() & flagBit) ^ flagBit ? '-' : '+');
    }
    delay(2000);

    clearDisplay();
    lcd.setCursor(2, 0);
    lcd.print("RESULT SAVED");
    lcd.setCursor(7, 1);
    lcd.print(":)");
    delay(600);

    clearDisplay();
}

/**
 * Does a cool led blinking show
 */
void blinkLeds() {
    for (const Todo& item: todos) {
        digitalWrite(item.ledPin, HIGH);
    }

    for (int i = 0; i < 3; ++i) {
        delay(800 - i * 200);
        digitalWrite(todos[2 + i].ledPin, LOW);
        digitalWrite(todos[2 - i].ledPin, LOW);
    }

    delay(200);

    for (int i = 0; i < 3; ++i) {
        for (const Todo& item: todos) {
            digitalWrite(item.ledPin, HIGH);
            delay(100);
            digitalWrite(item.ledPin, LOW);
        }
    }
}

/**
 * Saves result and shows information screens
 */
void saveAndReset() {
    EEPROM.update(getDayIndex(), todoDayResults());
    incrementDayIndex();
    
    lcd.backlight();
    showSaveValidation();
    showStats();
    lcd.noBacklight();

    blinkLeds();

    for (Todo& item: todos) {
        item.isDone = false;
    }

    // Reset save switch position in case it was moved
    saveSwitch.lastState = digitalRead(saveSwitch.pin);
}

/**
 * Shows message after initializing Arduino
 */
void showInitMsg() {
    lcd.backlight();
    lcd.setCursor(4, 0);
    lcd.print("TODO BOX");
    lcd.setCursor(5, 1);
    lcd.print("BI-ARD");
    delay(1000);

    clearDisplay();
    lcd.setCursor(0, 0);
    lcd.print("DAY: ");
    lcd.print(getDayIndex() - firstDayIndex + 1);
    lcd.setCursor(0,1);
    lcd.print("STREAK: ");
    lcd.print(countCurrentStreak(0b00011111));
    delay(1000);

    clearDisplay();
    lcd.noBacklight();
}


// ----------------------------------------------
// MAIN BODY OF ARDUINO -------------------------
// ----------------------------------------------

void setup() {
    // PIN ALL BUTTONS AND LEDs
    for (const Todo& item: todos) {
        pinMode(item.ledPin, OUTPUT);
        pinMode(item.btn.pin, INPUT_PULLUP);
    }
    pinMode(saveSwitch.pin, INPUT_PULLUP);
    pinMode(mainLed.redPin, OUTPUT);
    pinMode(mainLed.greenPin, OUTPUT);

    // INIT DISPLAY
    lcd.init();

    setMainLed();
    showInitMsg();
    blinkLeds();

    saveSwitch.lastState = digitalRead(saveSwitch.pin);
}


void loop() {
      for (Todo& item: todos) {
        if (!buttonPressed(item.btn)) { continue; }

        item.isDone = !item.isDone;
        digitalWrite(item.ledPin, item.isDone ? HIGH : LOW);
        setMainLed();
        showClickMsg(item);
    }

    if (switchChanged(saveSwitch)) {
        saveAndReset();
    }
}
