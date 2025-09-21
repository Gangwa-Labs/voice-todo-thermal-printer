#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <EEPROM.h>
#include "credentials.h"  // Include WiFi credentials (not committed to Git)

// Thermal printer configuration
const int printerBaudrate = 9600;
const byte rxPin = 44;  // XIAO ESP32S3 RX pin
const byte txPin = 43;  // XIAO ESP32S3 TX pin

HardwareSerial printerSerial(1);
WebServer server(80);

// Core habits storage configuration
#define EEPROM_SIZE 512
#define CORE_HABITS_START_ADDR 0
#define MAX_HABIT_NAME_LENGTH 50
#define MAX_CORE_HABITS 10

// Core habits structure
struct CoreHabit {
  char name[MAX_HABIT_NAME_LENGTH];
  bool enabled;
};

// Core habits array
CoreHabit coreHabits[MAX_CORE_HABITS];
int coreHabitsCount = 0;

// Date arrays
const char* months[] = {"January", "February", "March", "April", "May", "June",
                       "July", "August", "September", "October", "November", "December"};

const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

// Encouragement phrases (30 total)
const char* encouragementPhrases[] = {
  "Every small step counts!",
  "You're building greatness daily.",
  "Consistency creates magic.",
  "Your future self will thank you.",
  "Progress, not perfection.",
  "One day at a time.",
  "You've got this!",
  "Small habits, big results.",
  "Keep going, you're amazing!",
  "Success is a daily practice.",
  "Believe in your journey.",
  "You are unstoppable!",
  "Focus on the process.",
  "Growth happens gradually.",
  "Celebrate every victory.",
  "You're stronger than you think.",
  "Persistence pays off.",
  "Your dedication inspires.",
  "Keep pushing forward!",
  "Excellence is a habit.",
  "You're creating positive change.",
  "Stay committed to yourself.",
  "Every day is a new chance.",
  "You're worth the effort.",
  "Trust the process.",
  "Your goals are achievable.",
  "Momentum builds success.",
  "You're becoming your best self.",
  "Habits shape your destiny.",
  "Keep building, keep growing!"
};

// Core habits management functions
void initializeCoreHabits() {
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Try to load existing core habits from EEPROM
  loadCoreHabits();

  // If no habits found, initialize with defaults
  if (coreHabitsCount == 0) {
    setupDefaultCoreHabits();
    saveCoreHabits();
  }
}

void setupDefaultCoreHabits() {
  const char* defaultHabits[] = {"Breakfast", "Lunch", "Dinner", "Meditation", "Exercise"};
  coreHabitsCount = 5;

  for (int i = 0; i < coreHabitsCount; i++) {
    strncpy(coreHabits[i].name, defaultHabits[i], MAX_HABIT_NAME_LENGTH - 1);
    coreHabits[i].name[MAX_HABIT_NAME_LENGTH - 1] = '\0';
    coreHabits[i].enabled = true;
  }
}

void saveCoreHabits() {
  int addr = CORE_HABITS_START_ADDR;

  // Save count
  EEPROM.write(addr, coreHabitsCount);
  addr++;

  // Save each habit
  for (int i = 0; i < coreHabitsCount; i++) {
    EEPROM.put(addr, coreHabits[i]);
    addr += sizeof(CoreHabit);
  }

  EEPROM.commit();
  Serial.println("Core habits saved to EEPROM");
}

void loadCoreHabits() {
  int addr = CORE_HABITS_START_ADDR;

  // Load count
  coreHabitsCount = EEPROM.read(addr);
  addr++;

  // Validate count
  if (coreHabitsCount < 0 || coreHabitsCount > MAX_CORE_HABITS) {
    coreHabitsCount = 0;
    return;
  }

  // Load each habit
  for (int i = 0; i < coreHabitsCount; i++) {
    EEPROM.get(addr, coreHabits[i]);
    addr += sizeof(CoreHabit);
  }

  Serial.println("Core habits loaded from EEPROM: " + String(coreHabitsCount) + " habits");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize core habits system
  initializeCoreHabits();

  // Initialize thermal printer
  printerSerial.begin(printerBaudrate, SERIAL_8N1, rxPin, txPin);
  delay(1000);
  initializePrinter();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/print-todo", HTTP_POST, handlePrintTodo);
  server.on("/print-simple", HTTP_POST, handlePrintSimple);
  server.on("/print-habit", HTTP_POST, handlePrintHabit);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/core-habits", HTTP_GET, handleGetCoreHabits);
  server.on("/core-habits", HTTP_POST, handleUpdateCoreHabits);

  server.enableCORS(true);
  server.begin();
  Serial.println("HTTP server started - To-Do List & Habit Tracker Ready!");
}

void loop() {
  server.handleClient();
}

void initializePrinter() {
  Serial.println("Initializing printer...");

  // ESC @ - Initialize printer
  printerSerial.write(0x1B);
  printerSerial.write('@');
  delay(500);

  // Set print density and heat
  printerSerial.write(0x1B);
  printerSerial.write(0x37);
  printerSerial.write(0x07);
  printerSerial.write(0x28);
  printerSerial.write(0x28);
  delay(200);

  Serial.println("Printer initialized");
}

void printLine(String text) {
  // Split long text into lines if needed to prevent word cutoff
  int lineWidth = 32; // Approximate characters per line for thermal printer
  if (text.length() <= lineWidth) {
    printerSerial.print(text);
    printerSerial.write('\n');
  } else {
    int start = 0;
    while (start < text.length()) {
      int end = start + lineWidth;
      if (end >= text.length()) {
        // Last part of the text
        printerSerial.print(text.substring(start));
        printerSerial.write('\n');
        break;
      } else {
        // Find last space before line limit to avoid cutting words
        int lastSpace = text.lastIndexOf(' ', end);
        if (lastSpace > start) {
          end = lastSpace;
        }
        printerSerial.print(text.substring(start, end));
        printerSerial.write('\n');
        start = end + 1; // Skip the space
      }
    }
  }
}

void setAlignment(char align) {
  printerSerial.write(0x1B);
  printerSerial.write('a');
  switch(align) {
    case 'C': case 'c':
      printerSerial.write(1); // Center
      break;
    case 'R': case 'r':
      printerSerial.write(2); // Right
      break;
    default:
      printerSerial.write(0); // Left
      break;
  }
  delay(50);
}

void setBold(bool bold) {
  printerSerial.write(0x1B);
  printerSerial.write('E');
  printerSerial.write(bold ? 1 : 0);
  delay(50);
}

void setTextSize(char size) {
  printerSerial.write(0x1D);
  printerSerial.write('!');
  switch(size) {
    case 'L': case 'l':
      printerSerial.write(0x11); // Large
      break;
    case 'S': case 's':
      printerSerial.write(0x00); // Small
      break;
    default:
      printerSerial.write(0x00); // Normal
      break;
  }
  delay(50);
}

void advancePaper(int lines) {
  for(int i = 0; i < lines; i++) {
    printerSerial.write('\n');
  }
}

void handleRoot() {
  String html = "";
  html += "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<title>Daily To-Do & Habit Tracker</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  // Base styles
  html += "body { font-family: Arial, sans-serif; max-width: 700px; margin: 0 auto; padding: 20px; background-color: #f5f5f5; }";
  html += ".container { background-color: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += "h1 { color: #333; text-align: center; margin-bottom: 30px; }";

  // Tab styles
  html += ".tab-container { border-bottom: 2px solid #dee2e6; margin-bottom: 30px; }";
  html += ".tab-buttons { display: flex; }";
  html += ".tab-button { background: none; border: none; padding: 15px 25px; cursor: pointer; font-size: 16px; color: #6c757d; border-bottom: 3px solid transparent; }";
  html += ".tab-button.active { color: #007bff; border-bottom-color: #007bff; font-weight: bold; }";
  html += ".tab-button:hover { color: #007bff; }";
  html += ".tab-content { display: none; }";
  html += ".tab-content.active { display: block; }";

  // Form styles
  html += ".date-section { background-color: #e9ecef; padding: 15px; border-radius: 5px; margin-bottom: 20px; }";
  html += ".form-group { margin-bottom: 15px; }";
  html += "label { display: block; margin-bottom: 5px; font-weight: bold; color: #555; }";
  html += "input, select, textarea { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 5px; font-size: 16px; box-sizing: border-box; }";
  html += "textarea { height: 80px; resize: vertical; }";
  html += ".todo-item { display: flex; align-items: center; margin-bottom: 10px; padding: 10px; background-color: #f8f9fa; border-radius: 5px; cursor: move; }";
  html += ".todo-item.dragging { opacity: 0.5; }";
  html += ".todo-item input[type='text'] { flex: 1; margin-right: 10px; }";
  html += ".drag-handle { margin-right: 10px; cursor: grab; user-select: none; color: #6c757d; font-size: 18px; display: flex; align-items: center; justify-content: center; width: 20px; height: 20px; }";
  html += ".drag-handle:active { cursor: grabbing; }";
  html += ".drag-handle:hover { background-color: #e9ecef; border-radius: 3px; }";
  html += ".remove-btn { background-color: #dc3545; color: white; border: none; padding: 5px 10px; border-radius: 3px; cursor: pointer; }";
  html += ".remove-btn:hover { background-color: #c82333; }";

  // Button styles
  html += "button { background-color: #007bff; color: white; padding: 12px 24px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; margin: 5px; }";
  html += "button:hover { background-color: #0056b3; }";
  html += ".add-btn { background-color: #28a745; }";
  html += ".add-btn:hover { background-color: #1e7e34; }";
  html += ".print-btn { background-color: #17a2b8; font-size: 18px; padding: 15px 30px; }";
  html += ".print-btn:hover { background-color: #138496; }";

  // Status and preview styles
  html += ".status { margin-top: 20px; padding: 10px; border-radius: 5px; font-weight: bold; }";
  html += ".success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
  html += ".error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
  html += ".info { background-color: #d1ecf1; color: #0c5460; border: 1px solid #bee5eb; }";
  html += ".preview { background-color: #f8f9fa; padding: 15px; border-radius: 5px; margin: 20px 0; border-left: 4px solid #007bff; }";
  html += ".preview h3 { margin-top: 0; color: #007bff; }";
  html += ".preview-item { font-family: monospace; margin: 5px 0; }";
  html += ".checkbox-option { display: flex; align-items: center; margin: 10px 0; }";
  html += ".checkbox-option input { width: auto; margin-right: 10px; }";
  html += ".core-habit-item { display: flex; align-items: center; padding: 8px; background-color: #e8f4fd; border-radius: 4px; margin: 5px 0; }";
  html += ".core-habit-item input[type='checkbox'] { margin-right: 10px; }";
  html += ".core-habit-item input[type='text'] { flex: 1; border: 1px solid #ccc; padding: 5px; margin-right: 10px; }";
  html += ".core-habit-item button { background-color: #dc3545; color: white; border: none; padding: 4px 8px; border-radius: 3px; cursor: pointer; }";
  html += ".core-habit-item button:hover { background-color: #c82333; }";
  html += "</style></head><body>";

  html += "<div class='container'>";
  html += "<h1>Daily To-Do & Habit Tracker</h1>";

  // Tab navigation
  html += "<div class='tab-container'>";
  html += "<div class='tab-buttons'>";
  html += "<button class='tab-button active' onclick='showTab(\"todo\")'>Daily To-Do</button>";
  html += "<button class='tab-button' onclick='showTab(\"simple\")'>Simple List</button>";
  html += "<button class='tab-button' onclick='showTab(\"habit\")'>Habit Tracker</button>";
  html += "</div></div>";

  // To-Do Tab Content
  html += "<div id='todo-tab' class='tab-content active'>";
  html += "<div class='date-section'>";
  html += "<h3>Date Settings</h3>";
  html += "<div class='form-group'>";
  html += "<label for='customDate'>Select Date:</label>";
  html += "<input type='date' id='customDate'>";
  html += "</div>";
  html += "<button onclick='setToday()'>Use Today's Date</button>";
  html += "</div>";

  html += "<div class='form-group'>";
  html += "<label for='todoTitle'>List Title (optional):</label>";
  html += "<input type='text' id='todoTitle' placeholder='My Daily Tasks'>";
  html += "</div>";

  // Core habits section
  html += "<div class='form-group'>";
  html += "<label>Core Habits (always included):</label>";
  html += "<div id='coreHabits' style='margin-bottom: 15px;'>";
  html += "<div style='color: #6c757d; font-style: italic; padding: 10px;'>Loading core habits...</div>";
  html += "</div>";
  html += "<button type='button' onclick='editCoreHabits()' style='background-color: #6c757d; margin-bottom: 10px;'>Edit Core Habits</button>";
  html += "</div>";

  html += "<div class='form-group'>";
  html += "<label>To-Do Items (drag to reorder):</label>";
  html += "<div id='todoItems'>";
  html += "<div class='todo-item' draggable='true'>";
  html += "<span class='drag-handle'>=</span>";
  html += "<input type='text' placeholder='Enter a task...' class='todo-input'>";
  html += "<button class='remove-btn' onclick='removeTodoItem(this)'>Remove</button>";
  html += "</div>";
  html += "</div>";
  html += "<button class='add-btn' onclick='addTodoItem()'>+ Add Item</button>";
  html += "</div>";

  html += "<div class='preview' id='todo-preview'>";
  html += "<h3>Print Preview</h3>";
  html += "<div id='todoPreviewContent'>Select a date and add items to see preview</div>";
  html += "</div>";

  html += "<button class='print-btn' onclick='printTodoList()'>Print To-Do List</button>";
  html += "</div>";

  // Simple To-Do Tab Content
  html += "<div id='simple-tab' class='tab-content'>";

  html += "<div class='form-group'>";
  html += "<label for='simpleTitle'>List Title (optional):</label>";
  html += "<input type='text' id='simpleTitle' placeholder='My Simple List'>";
  html += "</div>";

  html += "<div class='form-group'>";
  html += "<label>Simple To-Do Items (drag to reorder):</label>";
  html += "<div id='simpleItems'>";
  html += "<div class='todo-item' draggable='true'>";
  html += "<span class='drag-handle'>=</span>";
  html += "<input type='text' placeholder='Enter a task...' class='simple-input'>";
  html += "<button class='remove-btn' onclick='removeSimpleItem(this)'>Remove</button>";
  html += "</div>";
  html += "</div>";
  html += "<button class='add-btn' onclick='addSimpleItem()'>+ Add Item</button>";
  html += "</div>";

  html += "<div class='preview' id='simple-preview'>";
  html += "<h3>Print Preview</h3>";
  html += "<div id='simplePreviewContent'>Add items to see preview</div>";
  html += "</div>";

  html += "<button class='print-btn' onclick='printSimpleList()'>Print Simple List</button>";
  html += "</div>";

  // Habit Tracker Tab Content
  html += "<div id='habit-tab' class='tab-content'>"
  html += "<div class='form-group'>";
  html += "<label for='habitName'>Habit Name:</label>";
  html += "<input type='text' id='habitName' placeholder='e.g., Drink 8 glasses of water'>";
  html += "</div>";

  html += "<div class='form-group'>";
  html += "<label for='habitTimeframe'>Time Frame:</label>";
  html += "<select id='habitTimeframe'>";
  html += "<option value='14'>2 Weeks (14 days)</option>";
  html += "<option value='30'>1 Month (30 days)</option>";
  html += "</select>";
  html += "</div>";

  html += "<div class='form-group'>";
  html += "<label for='habitWhy'>Why & Who You Become:</label>";
  html += "<textarea id='habitWhy' placeholder='Explain why you want this habit and who it helps you become...'></textarea>";
  html += "</div>";

  html += "<div class='form-group'>";
  html += "<label for='habitTrigger'>When/Trigger:</label>";
  html += "<input type='text' id='habitTrigger' placeholder='e.g., After I wake up, Before lunch, After dinner'>";
  html += "</div>";

  html += "<div class='form-group'>";
  html += "<label for='habitStartDate'>Start Date:</label>";
  html += "<input type='date' id='habitStartDate'>";
  html += "</div>";

  html += "<div class='checkbox-option'>";
  html += "<input type='checkbox' id='showDayNumbers'>";
  html += "<label for='showDayNumbers'>Show upcoming dates instead of day numbers</label>";
  html += "</div>";

  html += "<div class='preview' id='habit-preview'>";
  html += "<h3>Habit Card Preview</h3>";
  html += "<div id='habitPreviewContent'>Fill in the habit details to see preview</div>";
  html += "</div>";

  html += "<button class='print-btn' onclick='printHabitCard()'>Print Habit Card</button>";
  html += "</div>";

  html += "<div id='status'></div></div>";

  // JavaScript
  html += "<script>";

  // Drag and drop functionality with mobile support
  html += "let draggedElement = null;";
  html += "let touchStartY = 0;";
  html += "let touchElement = null;";
  html += "function enableDragAndDrop() {";
  html += "const items = document.querySelectorAll('.todo-item');";
  html += "items.forEach(item => {";

  // Desktop drag events
  html += "item.addEventListener('dragstart', function(e) {";
  html += "draggedElement = this;";
  html += "this.classList.add('dragging');";
  html += "});";
  html += "item.addEventListener('dragend', function(e) {";
  html += "this.classList.remove('dragging');";
  html += "updateTodoPreview();";
  html += "});";
  html += "item.addEventListener('dragover', function(e) {";
  html += "e.preventDefault();";
  html += "});";
  html += "item.addEventListener('drop', function(e) {";
  html += "e.preventDefault();";
  html += "if (draggedElement && draggedElement !== this) {";
  html += "const container = document.getElementById('todoItems');";
  html += "const allItems = Array.from(container.children);";
  html += "const draggedIndex = allItems.indexOf(draggedElement);";
  html += "const targetIndex = allItems.indexOf(this);";
  html += "if (draggedIndex < targetIndex) {";
  html += "container.insertBefore(draggedElement, this.nextSibling);";
  html += "} else {";
  html += "container.insertBefore(draggedElement, this);";
  html += "}";
  html += "updateTodoPreview();";
  html += "}";
  html += "});";

  // Mobile touch events
  html += "const handle = item.querySelector('.drag-handle');";
  html += "if (handle) {";
  html += "handle.addEventListener('touchstart', function(e) {";
  html += "e.preventDefault();";
  html += "touchStartY = e.touches[0].clientY;";
  html += "touchElement = item;";
  html += "item.classList.add('dragging');";
  html += "}, {passive: false});";
  html += "}";

  html += "});";

  html += "document.addEventListener('touchmove', function(e) {";
  html += "if (touchElement) {";
  html += "e.preventDefault();";
  html += "const touchY = e.touches[0].clientY;";
  html += "const container = document.getElementById('todoItems');";
  html += "const items = Array.from(container.children);";
  html += "const elementBelow = document.elementFromPoint(e.touches[0].clientX, touchY);";
  html += "const targetItem = elementBelow ? elementBelow.closest('.todo-item') : null;";
  html += "if (targetItem && targetItem !== touchElement) {";
  html += "const targetIndex = items.indexOf(targetItem);";
  html += "const touchIndex = items.indexOf(touchElement);";
  html += "if (touchY > touchStartY && touchIndex < targetIndex) {";
  html += "container.insertBefore(touchElement, targetItem.nextSibling);";
  html += "} else if (touchY < touchStartY && touchIndex > targetIndex) {";
  html += "container.insertBefore(touchElement, targetItem);";
  html += "}";
  html += "}";
  html += "}";
  html += "}, {passive: false});";

  html += "document.addEventListener('touchend', function(e) {";
  html += "if (touchElement) {";
  html += "touchElement.classList.remove('dragging');";
  html += "touchElement = null;";
  html += "updateTodoPreview();";
  html += "}";
  html += "});";

  html += "}";

  // Tab switching
  html += "function showTab(tabName) {";
  html += "document.querySelectorAll('.tab-content').forEach(tab => tab.classList.remove('active'));";
  html += "document.querySelectorAll('.tab-button').forEach(btn => btn.classList.remove('active'));";
  html += "document.getElementById(tabName + '-tab').classList.add('active');";
  html += "event.target.classList.add('active');";
  html += "}";

  // To-Do functions
  html += "function setToday() {";
  html += "const today = new Date();";
  html += "const dateStr = today.getFullYear() + '-' + String(today.getMonth() + 1).padStart(2, '0') + '-' + String(today.getDate()).padStart(2, '0');";
  html += "document.getElementById('customDate').value = dateStr;";
  html += "updateTodoPreview();";
  html += "}";

  html += "function addTodoItem() {";
  html += "const container = document.getElementById('todoItems');";
  html += "const newItem = document.createElement('div');";
  html += "newItem.className = 'todo-item';";
  html += "newItem.draggable = true;";
  html += "newItem.innerHTML = '<span class=\"drag-handle\">=</span><input type=\"text\" placeholder=\"Enter a task...\" class=\"todo-input\"><button class=\"remove-btn\" onclick=\"removeTodoItem(this)\">Remove</button>';";
  html += "container.appendChild(newItem);";
  html += "enableDragAndDrop();";
  html += "updateTodoPreview();";
  html += "}";

  html += "function removeTodoItem(btn) {";
  html += "btn.parentElement.remove();";
  html += "updateTodoPreview();";
  html += "}";

  html += "function updateTodoPreview() {";
  html += "const date = document.getElementById('customDate').value;";
  html += "const title = document.getElementById('todoTitle').value;";
  html += "const items = Array.from(document.querySelectorAll('.todo-input')).map(input => input.value).filter(val => val.trim());";
  html += "let preview = '';";
  html += "if (date) {";
  html += "const dateObj = new Date(date + 'T00:00:00');";
  html += "const months = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];";
  html += "const days = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];";
  html += "preview += '<div class=\"preview-item\">====================</div>';";
  html += "preview += '<div class=\"preview-item\">     DAILY TO-DO LIST</div>';";
  html += "preview += '<div class=\"preview-item\">====================</div>';";
  html += "preview += '<div class=\"preview-item\"></div>';";
  html += "preview += '<div class=\"preview-item\">' + String(dateObj.getDate()).padStart(2, '0') + '/' + String(dateObj.getMonth() + 1).padStart(2, '0') + '</div>';";
  html += "preview += '<div class=\"preview-item\">' + days[dateObj.getDay()] + ', ' + months[dateObj.getMonth()] + ' ' + dateObj.getDate() + '</div>';";
  html += "preview += '<div class=\"preview-item\"></div>';";
  html += "if (title) preview += '<div class=\"preview-item\">' + title + '</div><div class=\"preview-item\"></div>';";
  html += "preview += '<div class=\"preview-item\">--------------------</div>';";

  // Add core habits first
  html += "const enabledCoreHabits = coreHabitsData.filter(h => h.enabled);";
  html += "if (enabledCoreHabits.length > 0) {";
  html += "preview += '<div class=\"preview-item\" style=\"font-weight: bold; color: #007bff;\">CORE HABITS:</div>';";
  html += "enabledCoreHabits.forEach(habit => {";
  html += "preview += '<div class=\"preview-item\">[ ] ' + habit.name + '</div>';";
  html += "});";
  html += "if (items.length > 0) preview += '<div class=\"preview-item\"></div><div class=\"preview-item\" style=\"font-weight: bold; color: #007bff;\">TASKS:</div>';";
  html += "}";

  // Add regular todo items
  html += "items.forEach(item => { preview += '<div class=\"preview-item\">[ ] ' + item + '</div>'; });";
  html += "preview += '<div class=\"preview-item\">--------------------</div>';";
  html += "} else { preview = 'Select a date to see preview'; }";
  html += "document.getElementById('todoPreviewContent').innerHTML = preview;";
  html += "}";

  // Simple To-Do functions
  html += "function addSimpleItem() {";
  html += "const container = document.getElementById('simpleItems');";
  html += "const newItem = document.createElement('div');";
  html += "newItem.className = 'todo-item';";
  html += "newItem.draggable = true;";
  html += "newItem.innerHTML = '<span class=\"drag-handle\">=</span><input type=\"text\" placeholder=\"Enter a task...\" class=\"simple-input\"><button class=\"remove-btn\" onclick=\"removeSimpleItem(this)\">Remove</button>';";
  html += "container.appendChild(newItem);";
  html += "enableDragAndDrop();";
  html += "updateSimplePreview();";
  html += "}";

  html += "function removeSimpleItem(btn) {";
  html += "btn.parentElement.remove();";
  html += "updateSimplePreview();";
  html += "}";

  html += "function updateSimplePreview() {";
  html += "const title = document.getElementById('simpleTitle').value;";
  html += "const items = Array.from(document.querySelectorAll('.simple-input')).map(input => input.value).filter(val => val.trim());";
  html += "let preview = '';";
  html += "preview += '<div class=\"preview-item\">===================</div>';";
  html += "preview += '<div class=\"preview-item\">     TO-DO LIST</div>';";
  html += "preview += '<div class=\"preview-item\">===================</div>';";
  html += "preview += '<div class=\"preview-item\"></div>';";
  html += "if (title) preview += '<div class=\"preview-item\">' + title + '</div><div class=\"preview-item\"></div>';";
  html += "preview += '<div class=\"preview-item\">--------------------</div>';";
  html += "items.forEach(item => { preview += '<div class=\"preview-item\">[ ] ' + item + '</div>'; });";
  html += "if (items.length === 0) preview += '<div class=\"preview-item\">Add some tasks above!</div>';";
  html += "preview += '<div class=\"preview-item\">--------------------</div>';";
  html += "document.getElementById('simplePreviewContent').innerHTML = preview;";
  html += "}";

  html += "function printSimpleList() {";
  html += "const title = document.getElementById('simpleTitle').value;";
  html += "const items = Array.from(document.querySelectorAll('.simple-input')).map(input => input.value).filter(val => val.trim());";
  html += "if (items.length === 0) { showStatus('Please add at least one item!', 'error'); return; }";
  html += "showStatus('Printing simple list...', 'info');";
  html += "fetch('/print-simple', {";
  html += "method: 'POST',";
  html += "headers: { 'Content-Type': 'application/json' },";
  html += "body: JSON.stringify({ title: title, items: items })";
  html += "}).then(function(response) {";
  html += "if (response.ok) showStatus('Simple list printed successfully!', 'success');";
  html += "else showStatus('Print failed. Check printer connection.', 'error');";
  html += "}).catch(function(error) { showStatus('Error: ' + error.message, 'error'); });";
  html += "}";

  // Habit tracker functions
  html += "function setHabitStartToday() {";
  html += "const today = new Date();";
  html += "const dateStr = today.getFullYear() + '-' + String(today.getMonth() + 1).padStart(2, '0') + '-' + String(today.getDate()).padStart(2, '0');";
  html += "document.getElementById('habitStartDate').value = dateStr;";
  html += "updateHabitPreview();";
  html += "}";

  html += "function updateHabitPreview() {";
  html += "const name = document.getElementById('habitName').value;";
  html += "const timeframe = parseInt(document.getElementById('habitTimeframe').value);";
  html += "const why = document.getElementById('habitWhy').value;";
  html += "const trigger = document.getElementById('habitTrigger').value;";
  html += "const startDate = document.getElementById('habitStartDate').value;";
  html += "const showDates = document.getElementById('showDayNumbers').checked;";
  html += "let preview = '';";
  html += "if (name && why && trigger && startDate) {";
  html += "const startObj = new Date(startDate + 'T00:00:00');";
  html += "const endObj = new Date(startObj);";
  html += "endObj.setDate(startObj.getDate() + timeframe - 1);";
  html += "preview += '<div class=\"preview-item\">====================</div>';";
  html += "preview += '<div class=\"preview-item\">     HABIT TRACKER</div>';";
  html += "preview += '<div class=\"preview-item\">====================</div>';";
  html += "preview += '<div class=\"preview-item\"></div>';";
  html += "preview += '<div class=\"preview-item\">' + String(startObj.getMonth() + 1).padStart(2, '0') + '/' + String(startObj.getDate()).padStart(2, '0') + ' - ' + String(endObj.getMonth() + 1).padStart(2, '0') + '/' + String(endObj.getDate()).padStart(2, '0') + '</div>';";
  html += "preview += '<div class=\"preview-item\"></div>';";
  html += "preview += '<div class=\"preview-item\">HABIT: ' + name + '</div>';";
  html += "preview += '<div class=\"preview-item\"></div>';";
  html += "preview += '<div class=\"preview-item\">WHY: ' + why + '</div>';";
  html += "preview += '<div class=\"preview-item\"></div>';";
  html += "preview += '<div class=\"preview-item\">TRIGGER: ' + trigger + '</div>';";
  html += "preview += '<div class=\"preview-item\"></div>';";
  html += "preview += '<div class=\"preview-item\">--------------------</div>';";
  html += "let boxLine = '';";
  html += "for (let i = 1; i <= timeframe; i++) {";
  html += "if (showDates) {";
  html += "const currentDate = new Date(startObj);";
  html += "currentDate.setDate(startObj.getDate() + i - 1);";
  html += "boxLine += '[' + String(currentDate.getDate()).padStart(2, ' ') + '] ';";
  html += "} else {";
  html += "boxLine += '[' + String(i).padStart(2, ' ') + '] ';";
  html += "}";
  html += "if (i % 7 === 0 || i === timeframe) { preview += '<div class=\"preview-item\">' + boxLine + '</div>'; boxLine = ''; }";
  html += "}";
  html += "preview += '<div class=\"preview-item\">--------------------</div>';";
  html += "preview += '<div class=\"preview-item\">Keep going! You\\'ve got this!</div>';";
  html += "} else { preview = 'Fill in all fields to see preview'; }";
  html += "document.getElementById('habitPreviewContent').innerHTML = preview;";
  html += "}";

  // Print functions
  html += "function showStatus(message, type) {";
  html += "const statusDiv = document.getElementById('status');";
  html += "statusDiv.innerHTML = '<div class=\"status ' + type + '\">' + message + '</div>';";
  html += "}";

  html += "function printTodoList() {";
  html += "const date = document.getElementById('customDate').value;";
  html += "const title = document.getElementById('todoTitle').value;";
  html += "const items = Array.from(document.querySelectorAll('.todo-input')).map(input => input.value).filter(val => val.trim());";
  html += "const coreHabits = coreHabitsData.filter(h => h.enabled).map(h => h.name);";
  html += "if (!date) { showStatus('Please select a date first!', 'error'); return; }";
  html += "if (items.length === 0 && coreHabits.length === 0) { showStatus('Please add at least one item or enable core habits!', 'error'); return; }";
  html += "showStatus('Printing to-do list...', 'info');";
  html += "fetch('/print-todo', {";
  html += "method: 'POST',";
  html += "headers: { 'Content-Type': 'application/json' },";
  html += "body: JSON.stringify({ date: date, title: title, items: items, coreHabits: coreHabits })";
  html += "}).then(function(response) {";
  html += "if (response.ok) showStatus('To-do list printed successfully!', 'success');";
  html += "else showStatus('Print failed. Check printer connection.', 'error');";
  html += "}).catch(function(error) { showStatus('Error: ' + error.message, 'error'); });";
  html += "}";

  html += "function printHabitCard() {";
  html += "const name = document.getElementById('habitName').value;";
  html += "const timeframe = document.getElementById('habitTimeframe').value;";
  html += "const why = document.getElementById('habitWhy').value;";
  html += "const trigger = document.getElementById('habitTrigger').value;";
  html += "const startDate = document.getElementById('habitStartDate').value;";
  html += "const showDates = document.getElementById('showDayNumbers').checked;";
  html += "if (!name || !why || !trigger || !startDate) { showStatus('Please fill in all habit fields!', 'error'); return; }";
  html += "showStatus('Printing habit card...', 'info');";
  html += "fetch('/print-habit', {";
  html += "method: 'POST',";
  html += "headers: { 'Content-Type': 'application/json' },";
  html += "body: JSON.stringify({ name: name, timeframe: parseInt(timeframe), why: why, trigger: trigger, startDate: startDate, showDates: showDates })";
  html += "}).then(function(response) {";
  html += "if (response.ok) showStatus('Habit card printed successfully!', 'success');";
  html += "else showStatus('Print failed. Check printer connection.', 'error');";
  html += "}).catch(function(error) { showStatus('Error: ' + error.message, 'error'); });";
  html += "}";

  // Core habits management
  html += "let coreHabitsData = [];";
  html += "let editingCoreHabits = false;";

  html += "function loadCoreHabits() {";
  html += "fetch('/core-habits')";
  html += ".then(response => response.json())";
  html += ".then(data => {";
  html += "coreHabitsData = data;";
  html += "displayCoreHabits();";
  html += "updateTodoPreview();";
  html += "})";
  html += ".catch(error => console.error('Error loading core habits:', error));";
  html += "}";

  html += "function displayCoreHabits() {";
  html += "const container = document.getElementById('coreHabits');";
  html += "if (editingCoreHabits) return;";
  html += "let html = '';";
  html += "coreHabitsData.forEach(habit => {";
  html += "if (habit.enabled) {";
  html += "html += '<div style=\"padding: 5px; background-color: #f0f8ff; border-radius: 3px; margin: 2px 0; font-size: 14px;\">• ' + habit.name + '</div>';";
  html += "}";
  html += "});";
  html += "if (html === '') html = '<div style=\"color: #6c757d; font-style: italic;\">No core habits enabled</div>';";
  html += "container.innerHTML = html;";
  html += "}";

  html += "function editCoreHabits() {";
  html += "editingCoreHabits = true;";
  html += "const container = document.getElementById('coreHabits');";
  html += "let html = '';";
  html += "coreHabitsData.forEach((habit, index) => {";
  html += "html += '<div class=\"core-habit-item\">';";
  html += "html += '<input type=\"checkbox\" ' + (habit.enabled ? 'checked' : '') + ' onchange=\"toggleCoreHabit(' + index + ')\">';";
  html += "html += '<input type=\"text\" value=\"' + habit.name + '\" onchange=\"updateCoreHabitName(' + index + ', this.value)\">';";
  html += "html += '<button onclick=\"removeCoreHabit(' + index + ')\">Remove</button>';";
  html += "html += '</div>';";
  html += "});";
  html += "html += '<div style=\"margin-top: 10px;\">';";
  html += "html += '<button onclick=\"addCoreHabit()\" style=\"background-color: #28a745; margin-right: 10px;\">+ Add Habit</button>';";
  html += "html += '<button onclick=\"saveCoreHabits()\" style=\"background-color: #007bff; margin-right: 10px;\">Save Changes</button>';";
  html += "html += '<button onclick=\"cancelEditCoreHabits()\" style=\"background-color: #6c757d;\">Cancel</button>';";
  html += "html += '</div>';";
  html += "container.innerHTML = html;";
  html += "}";

  html += "function toggleCoreHabit(index) {";
  html += "coreHabitsData[index].enabled = !coreHabitsData[index].enabled;";
  html += "}";

  html += "function updateCoreHabitName(index, newName) {";
  html += "coreHabitsData[index].name = newName;";
  html += "}";

  html += "function addCoreHabit() {";
  html += "coreHabitsData.push({name: 'New Habit', enabled: true});";
  html += "editCoreHabits();";
  html += "}";

  html += "function removeCoreHabit(index) {";
  html += "coreHabitsData.splice(index, 1);";
  html += "editCoreHabits();";
  html += "}";

  html += "function saveCoreHabits() {";
  html += "fetch('/core-habits', {";
  html += "method: 'POST',";
  html += "headers: { 'Content-Type': 'application/json' },";
  html += "body: JSON.stringify(coreHabitsData)";
  html += "}).then(response => {";
  html += "if (response.ok) {";
  html += "editingCoreHabits = false;";
  html += "displayCoreHabits();";
  html += "updateTodoPreview();";
  html += "showStatus('Core habits saved successfully!', 'success');";
  html += "} else {";
  html += "showStatus('Failed to save core habits', 'error');";
  html += "}";
  html += "}).catch(error => {";
  html += "showStatus('Error saving core habits', 'error');";
  html += "});";
  html += "}";

  html += "function cancelEditCoreHabits() {";
  html += "editingCoreHabits = false;";
  html += "loadCoreHabits();";
  html += "}";

  // Event listeners
  html += "document.addEventListener('input', function() { updateTodoPreview(); updateSimplePreview(); updateHabitPreview(); });";
  html += "document.addEventListener('change', function() { updateTodoPreview(); updateSimplePreview(); updateHabitPreview(); });"
  html += "document.addEventListener('DOMContentLoaded', function() { enableDragAndDrop(); loadCoreHabits(); });";
  html += "setToday();";
  html += "setHabitStartToday();";
  html += "enableDragAndDrop();";
  html += "loadCoreHabits();";
  html += "</script></body></html>";

  server.send(200, "text/html; charset=UTF-8", html);
}

void handlePrintTodo() {
  if (server.hasArg("plain")) {
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));

    String dateStr = doc["date"];
    String title = doc["title"];
    JsonArray items = doc["items"];
    JsonArray coreHabits = doc["coreHabits"];

    Serial.println("Printing to-do list for date: " + dateStr);

    // Parse date
    int year = dateStr.substring(0, 4).toInt();
    int month = dateStr.substring(5, 7).toInt();
    int day = dateStr.substring(8, 10).toInt();

    // Calculate day of week (simplified algorithm)
    int dayOfWeek = calculateDayOfWeek(year, month, day);

    // Print header
    setAlignment('C');
    setBold(true);
    setTextSize('M');
    printLine("====================");
    printLine("     DAILY TO-DO LIST");
    printLine("====================");
    setBold(false);
    advancePaper(1);

    // Print date in numbers (DD/MM)
    setAlignment('L');
    String dateNumbers = "";
    if (day < 10) dateNumbers += "0";
    dateNumbers += String(day) + "/";
    if (month < 10) dateNumbers += "0";
    dateNumbers += String(month);
    printLine(dateNumbers);

    // Print date in English
    String englishDate = String(days[dayOfWeek]) + ", " + String(months[month-1]) + " " + String(day);
    printLine(englishDate);
    advancePaper(1);

    // Print title if provided
    if (title.length() > 0) {
      setBold(true);
      printLine(title);
      setBold(false);
      advancePaper(1);
    }

    // Print separator
    printLine("--------------------");

    // Print core habits first
    if (coreHabits.size() > 0) {
      setBold(true);
      printLine("CORE HABITS:");
      setBold(false);
      for (JsonVariant habit : coreHabits) {
        String habitItem = "[ ] " + habit.as<String>();
        printLine(habitItem);
      }

      // Add space between core habits and regular items
      if (items.size() > 0) {
        advancePaper(1);
        setBold(true);
        printLine("TASKS:");
        setBold(false);
      }
    }

    // Print regular to-do items with checkboxes
    for (JsonVariant item : items) {
      String todoItem = "[ ] " + item.as<String>();
      printLine(todoItem);
    }

    // Print footer
    printLine("--------------------");
    advancePaper(4);

    // Reset formatting
    setAlignment('L');
    setBold(false);
    setTextSize('M');

    server.send(200, "text/plain", "To-do list printed successfully");
  } else {
    server.send(400, "text/plain", "No data received");
  }
}

void handlePrintSimple() {
  if (server.hasArg("plain")) {
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));

    String title = doc["title"];
    JsonArray items = doc["items"];

    Serial.println("Printing simple list");

    // Print header
    setAlignment('C');
    setBold(true);
    setTextSize('M');
    printLine("====================");
    printLine("     TO-DO LIST");
    printLine("====================");
    setBold(false);
    advancePaper(1);

    // Print title if provided
    setAlignment('L');
    if (title.length() > 0) {
      setBold(true);
      printLine(title);
      setBold(false);
      advancePaper(1);
    }

    // Print separator
    printLine("--------------------");

    // Print to-do items with checkboxes
    for (JsonVariant item : items) {
      String todoItem = "[ ] " + item.as<String>();
      printLine(todoItem);
    }

    // Print footer
    printLine("--------------------");
    advancePaper(4);

    // Reset formatting
    setAlignment('L');
    setBold(false);
    setTextSize('M');

    server.send(200, "text/plain", "Simple list printed successfully");
  } else {
    server.send(400, "text/plain", "No data received");
  }
}

void handlePrintHabit() {
  if (server.hasArg("plain")) {
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));

    String name = doc["name"];
    int timeframe = doc["timeframe"];
    String why = doc["why"];
    String trigger = doc["trigger"];
    String startDateStr = doc["startDate"];
    bool showDates = doc["showDates"];

    Serial.println("Printing habit card: " + name);

    // Parse start date
    int year = startDateStr.substring(0, 4).toInt();
    int month = startDateStr.substring(5, 7).toInt();
    int day = startDateStr.substring(8, 10).toInt();

    // Calculate end date
    int endMonth = month;
    int endDay = day + timeframe - 1;

    // Handle month overflow (simplified)
    if (endDay > 30) {
      endDay -= 30;
      endMonth++;
      if (endMonth > 12) {
        endMonth = 1;
      }
    }

    // Print header
    setAlignment('C');
    setBold(true);
    setTextSize('M');
    printLine("====================");
    printLine("     HABIT TRACKER");
    printLine("====================");
    setBold(false);
    advancePaper(1);

    // Print date range
    setAlignment('L');
    String dateRange = "";
    if (month < 10) dateRange += "0";
    dateRange += String(month) + "/";
    if (day < 10) dateRange += "0";
    dateRange += String(day) + " - ";
    if (endMonth < 10) dateRange += "0";
    dateRange += String(endMonth) + "/";
    if (endDay < 10) dateRange += "0";
    dateRange += String(endDay);
    printLine(dateRange);
    advancePaper(1);

    // Print habit details
    setBold(true);
    printLine("HABIT: " + name);
    setBold(false);
    advancePaper(1);

    setBold(true);
    printLine("WHY: " + why);
    setBold(false);
    advancePaper(1);

    setBold(true);
    printLine("TRIGGER: " + trigger);
    setBold(false);
    advancePaper(1);

    printLine("--------------------");

    // Print tracking boxes
    String boxLine = "";
    for (int i = 1; i <= timeframe; i++) {
      if (showDates) {
        // Calculate actual date for this day
        int currentMonth = month;
        int currentDay = day + i - 1;

        // Handle month overflow
        if (currentDay > 30) {
          currentDay -= 30;
          currentMonth++;
          if (currentMonth > 12) {
            currentMonth = 1;
          }
        }

        boxLine += "[";
        if (currentDay < 10) boxLine += " ";
        boxLine += String(currentDay) + "] ";
      } else {
        boxLine += "[";
        if (i < 10) boxLine += " ";
        boxLine += String(i) + "] ";
      }

      // Print line every 7 boxes or at end
      if (i % 7 == 0 || i == timeframe) {
        printLine(boxLine);
        boxLine = "";
      }
    }

    printLine("--------------------");

    // Print random encouragement phrase
    int randomIndex = random(0, 30);
    setAlignment('C');
    printLine(encouragementPhrases[randomIndex]);
    advancePaper(4);

    // Reset formatting
    setAlignment('L');
    setBold(false);
    setTextSize('M');

    server.send(200, "text/plain", "Habit card printed successfully");
  } else {
    server.send(400, "text/plain", "No data received");
  }
}

void handleStatus() {
  String status = "To-Do List & Habit Tracker Ready";
  if (WiFi.status() == WL_CONNECTED) {
    status += " | WiFi: Connected";
  } else {
    status += " | WiFi: Disconnected";
  }
  status += " | IP: " + WiFi.localIP().toString();
  server.send(200, "text/plain", status);
}

void handleGetCoreHabits() {
  JsonDocument doc;
  JsonArray habitsArray = doc.to<JsonArray>();

  for (int i = 0; i < coreHabitsCount; i++) {
    JsonObject habit = habitsArray.add<JsonObject>();
    habit["name"] = coreHabits[i].name;
    habit["enabled"] = coreHabits[i].enabled;
  }

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleUpdateCoreHabits() {
  if (server.hasArg("plain")) {
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));

    JsonArray habitsArray = doc.as<JsonArray>();

    // Clear current habits
    coreHabitsCount = 0;

    // Add new habits (up to maximum)
    for (JsonVariant habitVar : habitsArray) {
      if (coreHabitsCount >= MAX_CORE_HABITS) break;

      JsonObject habit = habitVar.as<JsonObject>();
      String name = habit["name"].as<String>();
      bool enabled = habit["enabled"].as<bool>();

      if (name.length() > 0 && name.length() < MAX_HABIT_NAME_LENGTH) {
        strncpy(coreHabits[coreHabitsCount].name, name.c_str(), MAX_HABIT_NAME_LENGTH - 1);
        coreHabits[coreHabitsCount].name[MAX_HABIT_NAME_LENGTH - 1] = '\0';
        coreHabits[coreHabitsCount].enabled = enabled;
        coreHabitsCount++;
      }
    }

    // Save to EEPROM
    saveCoreHabits();

    server.send(200, "text/plain", "Core habits updated successfully");
    Serial.println("Core habits updated via API: " + String(coreHabitsCount) + " habits");
  } else {
    server.send(400, "text/plain", "No data received");
  }
}

// Calculate day of week (0=Sunday, 6=Saturday)
int calculateDayOfWeek(int year, int month, int day) {
  if (month < 3) {
    month += 12;
    year--;
  }
  int k = year % 100;
  int j = year / 100;
  int h = (day + ((13 * (month + 1)) / 5) + k + (k / 4) + (j / 4) - 2 * j) % 7;
  return (h + 5) % 7;  // Convert to 0=Sunday format
}