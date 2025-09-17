#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <driver/i2s.h>
#include <WiFiUdp.h>
#include "credentials.h"  // Include WiFi credentials (not committed to Git)

// Thermal printer configuration
const int printerBaudrate = 9600;
const byte rxPin = 44;  // XIAO ESP32S3 RX pin
const byte txPin = 43;  // XIAO ESP32S3 TX pin

// Dual INMP441 microphone configuration
const int I2S_WS = 6;   // Word Select (LRCLK) - shared
const int I2S_SCK = 5;  // Serial Clock (BCLK) - shared
const int I2S_SD = 9;   // Serial Data (DIN) - shared data line
const int BUTTON_PIN = 4; // Push button pin (active high)

// Audio configuration for stereo recording
const int SAMPLE_RATE = 16000;
const int BITS_PER_SAMPLE = 16;
const size_t BUFFER_SIZE = 512;   // Reduced to meet ESP32 I2S limits (max 1024)
const int DMA_BUF_COUNT = 4;      // Number of DMA buffers
const int CHANNELS = 2;           // Stereo for dual microphones

// Audio processing settings
const int NOISE_GATE_THRESHOLD = 300;  // Minimum amplitude to process
const int AUDIO_GAIN = 1;              // Reduced gain to prevent overflow

// Network configuration for audio streaming
// AUDIO_SERVER_IP is defined in credentials.h
const int AUDIO_SERVER_PORT = 8080;

HardwareSerial printerSerial(1);
WebServer server(80);
WiFiUDP udp;

// Audio streaming variables
bool isRecording = false;
bool buttonPressed = false;
bool lastButtonState = false;

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

void initializeI2S() {
  // Conservative I2S configuration to prevent crashes
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,  // Stereo for dual mics
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUF_COUNT,  // Use defined constant
    .dma_buf_len = BUFFER_SIZE,      // Within ESP32 limits (≤1024)
    .use_apll = false,               // Disable APLL to prevent instability
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  // Install I2S driver with error checking
  esp_err_t ret = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (ret != ESP_OK) {
    Serial.printf("Failed to install I2S driver: %s\n", esp_err_to_name(ret));
    Serial.println("Retrying with simpler configuration...");

    // Fallback to mono configuration
    i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    ret = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

    if (ret != ESP_OK) {
      Serial.printf("I2S driver install failed completely: %s\n", esp_err_to_name(ret));
      return;
    }
  }

  ret = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (ret != ESP_OK) {
    Serial.printf("Failed to set I2S pins: %s\n", esp_err_to_name(ret));
    return;
  }

  // Clear DMA buffers
  i2s_zero_dma_buffer(I2S_NUM_0);

  Serial.println("I2S initialized successfully");
  Serial.printf("Configuration: %dHz, %d-bit, %s\n",
                SAMPLE_RATE, BITS_PER_SAMPLE,
                (i2s_config.channel_format == I2S_CHANNEL_FMT_RIGHT_LEFT) ? "Stereo" : "Mono");
}

void handleButtonPress() {
  // Safe button handling with error checking
  bool currentButtonState = digitalRead(BUTTON_PIN);

  if (currentButtonState != lastButtonState) {
    if (currentButtonState == HIGH) {
      // Button pressed - start recording
      if (!isRecording) {
        Serial.println("Button pressed - starting audio recording");

        // Additional safety: ensure I2S is ready
        if (i2s_zero_dma_buffer(I2S_NUM_0) == ESP_OK) {
          isRecording = true;
          buttonPressed = true;
          Serial.println("Audio recording started successfully");
        } else {
          Serial.println("Warning: I2S buffer clear failed");
          isRecording = false;
        }
      }
    } else {
      // Button released - stop recording
      if (isRecording) {
        Serial.println("Button released - stopping audio recording");
        isRecording = false;
        buttonPressed = false;

        // Small delay to ensure last packets are sent
        delay(100);
      }
    }
    lastButtonState = currentButtonState;
    delay(50); // Debounce
  }
}

void streamAudioData() {
  if (!isRecording) return;

  size_t bytes_read = 0;
  int16_t audio_buffer[BUFFER_SIZE * CHANNELS];  // Buffer for stereo or mono data

  // Read audio data with timeout to prevent blocking
  esp_err_t result = i2s_read(I2S_NUM_0, audio_buffer, sizeof(audio_buffer), &bytes_read, 10 / portTICK_PERIOD_MS);

  if (result == ESP_OK && bytes_read > 0) {
    // Simple processing to prevent crashes
    size_t samples = bytes_read / 2;  // 16-bit samples

    // Convert stereo to mono and apply simple processing
    int16_t processed_buffer[BUFFER_SIZE];
    size_t output_samples = 0;

    for (size_t i = 0; i < samples; i += CHANNELS) {
      int32_t sample;

      if (CHANNELS == 2 && i + 1 < samples) {
        // Stereo: mix left and right channels
        sample = ((int32_t)audio_buffer[i] + (int32_t)audio_buffer[i + 1]) / 2;
      } else {
        // Mono: use single channel
        sample = audio_buffer[i];
      }

      // Apply simple gain
      sample *= AUDIO_GAIN;

      // Clip to prevent overflow
      if (sample > 32767) sample = 32767;
      if (sample < -32768) sample = -32768;

      processed_buffer[output_samples++] = (int16_t)sample;

      if (output_samples >= BUFFER_SIZE) break;
    }

    // Apply noise gate
    bool hasValidAudio = false;
    for (size_t i = 0; i < output_samples; i++) {
      if (abs(processed_buffer[i]) > NOISE_GATE_THRESHOLD) {
        hasValidAudio = true;
        break;
      }
    }

    // Send audio data if it passes noise gate
    if (hasValidAudio && output_samples > 0) {
      udp.beginPacket(AUDIO_SERVER_IP, AUDIO_SERVER_PORT);
      udp.write((uint8_t*)processed_buffer, output_samples * 2);
      udp.endPacket();
    }
  }
}

// Removed complex audio processing function to prevent crashes
// Audio processing is now simplified and integrated into streamAudioData()

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize button pin
  pinMode(BUTTON_PIN, INPUT);

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

  // Initialize I2S for microphone
  initializeI2S();
  Serial.println("INMP441 microphone initialized");

  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/print-todo", HTTP_POST, handlePrintTodo);
  server.on("/print-habit", HTTP_POST, handlePrintHabit);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/receive-text", HTTP_POST, handleReceiveText);

  server.enableCORS(true);
  server.begin();
  Serial.println("HTTP server started - To-Do List & Habit Tracker with Audio Ready!");
}

void loop() {
  server.handleClient();
  handleButtonPress();
  streamAudioData();
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
  printerSerial.print(text);
  printerSerial.write('\n');
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
  html += "<!DOCTYPE html><html><head><title>Daily To-Do & Habit Tracker</title>";
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
  html += ".todo-item { display: flex; align-items: center; margin-bottom: 10px; padding: 10px; background-color: #f8f9fa; border-radius: 5px; }";
  html += ".todo-item input[type='text'] { flex: 1; margin-right: 10px; }";
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
  html += "</style></head><body>";

  html += "<div class='container'>";
  html += "<h1>📝 Daily To-Do & Habit Tracker</h1>";

  // Tab navigation
  html += "<div class='tab-container'>";
  html += "<div class='tab-buttons'>";
  html += "<button class='tab-button active' onclick='showTab(\"todo\")'>📋 To-Do List</button>";
  html += "<button class='tab-button' onclick='showTab(\"habit\")'>🎯 Habit Tracker</button>";
  html += "</div></div>";

  // To-Do Tab Content
  html += "<div id='todo-tab' class='tab-content active'>";
  html += "<div class='date-section'>";
  html += "<h3>📅 Date Settings</h3>";
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

  html += "<div class='form-group'>";
  html += "<label>To-Do Items:</label>";
  html += "<div id='todoItems'>";
  html += "<div class='todo-item'>";
  html += "<input type='text' placeholder='Enter a task...' class='todo-input'>";
  html += "<button class='remove-btn' onclick='removeTodoItem(this)'>Remove</button>";
  html += "</div>";
  html += "</div>";
  html += "<button class='add-btn' onclick='addTodoItem()'>+ Add Item</button>";
  html += "</div>";

  html += "<div class='preview' id='todo-preview'>";
  html += "<h3>📄 Print Preview</h3>";
  html += "<div id='todoPreviewContent'>Select a date and add items to see preview</div>";
  html += "</div>";

  html += "<button class='print-btn' onclick='printTodoList()'>🖨️ Print To-Do List</button>";
  html += "</div>";

  // Habit Tracker Tab Content
  html += "<div id='habit-tab' class='tab-content'>";
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
  html += "<input type='checkbox' id='showDayNumbers' checked>";
  html += "<label for='showDayNumbers'>Show day numbers in boxes</label>";
  html += "</div>";

  html += "<div class='preview' id='habit-preview'>";
  html += "<h3>🎯 Habit Card Preview</h3>";
  html += "<div id='habitPreviewContent'>Fill in the habit details to see preview</div>";
  html += "</div>";

  html += "<button class='print-btn' onclick='printHabitCard()'>🖨️ Print Habit Card</button>";
  html += "</div>";

  html += "<div id='status'></div></div>";

  // JavaScript
  html += "<script>";

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
  html += "newItem.innerHTML = '<input type=\"text\" placeholder=\"Enter a task...\" class=\"todo-input\"><button class=\"remove-btn\" onclick=\"removeTodoItem(this)\">Remove</button>';";
  html += "container.appendChild(newItem);";
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
  html += "items.forEach(item => { preview += '<div class=\"preview-item\">[ ] ' + item + '</div>'; });";
  html += "preview += '<div class=\"preview-item\">--------------------</div>';";
  html += "} else { preview = 'Select a date to see preview'; }";
  html += "document.getElementById('todoPreviewContent').innerHTML = preview;";
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
  html += "const showNumbers = document.getElementById('showDayNumbers').checked;";
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
  html += "boxLine += showNumbers ? '[' + String(i).padStart(2, ' ') + '] ' : '[ ] ';";
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
  html += "if (!date) { showStatus('Please select a date first!', 'error'); return; }";
  html += "if (items.length === 0) { showStatus('Please add at least one to-do item!', 'error'); return; }";
  html += "showStatus('Printing to-do list...', 'info');";
  html += "fetch('/print-todo', {";
  html += "method: 'POST',";
  html += "headers: { 'Content-Type': 'application/json' },";
  html += "body: JSON.stringify({ date: date, title: title, items: items })";
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
  html += "const showNumbers = document.getElementById('showDayNumbers').checked;";
  html += "if (!name || !why || !trigger || !startDate) { showStatus('Please fill in all habit fields!', 'error'); return; }";
  html += "showStatus('Printing habit card...', 'info');";
  html += "fetch('/print-habit', {";
  html += "method: 'POST',";
  html += "headers: { 'Content-Type': 'application/json' },";
  html += "body: JSON.stringify({ name: name, timeframe: parseInt(timeframe), why: why, trigger: trigger, startDate: startDate, showNumbers: showNumbers })";
  html += "}).then(function(response) {";
  html += "if (response.ok) showStatus('Habit card printed successfully!', 'success');";
  html += "else showStatus('Print failed. Check printer connection.', 'error');";
  html += "}).catch(function(error) { showStatus('Error: ' + error.message, 'error'); });";
  html += "}";

  // Event listeners
  html += "document.addEventListener('input', function() { updateTodoPreview(); updateHabitPreview(); });";
  html += "document.addEventListener('change', function() { updateTodoPreview(); updateHabitPreview(); });";
  html += "setToday();";
  html += "setHabitStartToday();";
  html += "</script></body></html>";

  server.send(200, "text/html", html);
}

void handlePrintTodo() {
  if (server.hasArg("plain")) {
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));

    String dateStr = doc["date"];
    String title = doc["title"];
    JsonArray items = doc["items"];

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

    server.send(200, "text/plain", "To-do list printed successfully");
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
    bool showNumbers = doc["showNumbers"];

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
      if (showNumbers) {
        boxLine += "[";
        if (i < 10) boxLine += " ";
        boxLine += String(i) + "] ";
      } else {
        boxLine += "[ ] ";
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

void handleReceiveText() {
  if (server.hasArg("plain")) {
    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));

    String transcribedText = doc["text"];
    Serial.println("Received transcribed text: " + transcribedText);

    // Parse the transcribed text into todo items
    JsonArray todoItems = parseTodoItems(transcribedText);

    if (todoItems.size() > 0) {
      // Auto-print the todo list with today's date
      String todayDate = getCurrentDate();
      printAutoTodoList(todayDate, "Voice-to-Todo List", todoItems);

      server.send(200, "text/plain", "Todo list created and printed successfully");
      Serial.println("Auto-printed todo list with " + String(todoItems.size()) + " items");
    } else {
      server.send(200, "text/plain", "No todo items found in transcription");
      Serial.println("No todo items detected in transcription");
    }
  } else {
    server.send(400, "text/plain", "No text data received");
  }
}

JsonArray parseTodoItems(String text) {
  JsonDocument doc;
  JsonArray items = doc.to<JsonArray>();

  // Convert to lowercase for easier parsing
  text.toLowerCase();

  // Split text by common delimiters
  String delimiters[] = {", ", ". ", " and ", " then ", "\n"};
  int delimiterCount = 5;

  // Simple parsing - look for action words that typically start todo items
  String actionWords[] = {"buy", "get", "call", "email", "write", "finish", "complete", "start", "do", "make", "clean", "organize", "schedule", "plan", "review", "update", "check", "pay", "send", "pick up", "drop off", "visit", "go to", "remember to", "don't forget to"};
  int actionWordCount = 25;

  // Split the text into potential todo items
  int startIndex = 0;
  int textLength = text.length();

  for (int i = 0; i < textLength; i++) {
    bool foundDelimiter = false;

    // Check for delimiters
    for (int j = 0; j < delimiterCount; j++) {
      if (text.substring(i).startsWith(delimiters[j])) {
        String potentialItem = text.substring(startIndex, i);
        potentialItem.trim();

        // Check if this looks like a todo item
        if (isValidTodoItem(potentialItem, actionWords, actionWordCount)) {
          // Capitalize first letter
          if (potentialItem.length() > 0) {
            potentialItem.setCharAt(0, toupper(potentialItem.charAt(0)));
          }
          items.add(potentialItem);
        }

        startIndex = i + delimiters[j].length();
        i = startIndex - 1; // -1 because loop will increment
        foundDelimiter = true;
        break;
      }
    }
  }

  // Check the last segment
  String lastItem = text.substring(startIndex);
  lastItem.trim();
  if (isValidTodoItem(lastItem, actionWords, actionWordCount)) {
    if (lastItem.length() > 0) {
      lastItem.setCharAt(0, toupper(lastItem.charAt(0)));
    }
    items.add(lastItem);
  }

  return items;
}

bool isValidTodoItem(String item, String actionWords[], int actionWordCount) {
  item.trim();
  if (item.length() < 3) return false; // Too short to be meaningful

  // Check if item contains action words
  for (int i = 0; i < actionWordCount; i++) {
    if (item.indexOf(actionWords[i]) >= 0) {
      return true;
    }
  }

  // If no action words found, check if it's still a reasonable todo item
  // (has some basic structure like verb + object)
  return item.length() > 5; // Basic length check
}

String getCurrentDate() {
  // For now, return a placeholder - in a real implementation you'd get actual date
  // This could be enhanced to get real-time date from NTP server
  return "2024-01-01"; // Placeholder date format YYYY-MM-DD
}

void printAutoTodoList(String dateStr, String title, JsonArray items) {
  Serial.println("Auto-printing todo list for date: " + dateStr);

  // Parse date (using placeholder logic since we're using a fixed date)
  int year = 2024;
  int month = 1;
  int day = 1;

  // Calculate day of week
  int dayOfWeek = calculateDayOfWeek(year, month, day);

  // Print header
  setAlignment('C');
  setBold(true);
  setTextSize('M');
  printLine("====================");
  printLine("     VOICE TO-DO LIST");
  printLine("====================");
  setBold(false);
  advancePaper(1);

  // Print date
  setAlignment('L');
  String dateNumbers = "01/01"; // Placeholder
  printLine(dateNumbers);
  String englishDate = String(days[dayOfWeek]) + ", " + String(months[month-1]) + " " + String(day);
  printLine(englishDate);
  advancePaper(1);

  // Print title
  if (title.length() > 0) {
    setBold(true);
    printLine(title);
    setBold(false);
    advancePaper(1);
  }

  // Print separator
  printLine("--------------------");

  // Print todo items
  for (JsonVariant item : items) {
    String todoItem = "[ ] " + item.as<String>();
    printLine(todoItem);
  }

  // Print footer
  printLine("--------------------");

  // Add encouraging message
  setAlignment('C');
  printLine("Created from your voice!");
  advancePaper(4);

  // Reset formatting
  setAlignment('L');
  setBold(false);
  setTextSize('M');
}

void handleStatus() {
  String status = "To-Do List & Habit Tracker with Voice Ready";
  if (WiFi.status() == WL_CONNECTED) {
    status += " | WiFi: Connected";
  } else {
    status += " | WiFi: Disconnected";
  }
  status += " | IP: " + WiFi.localIP().toString();
  status += " | Audio Server: " + String(AUDIO_SERVER_IP) + ":" + String(AUDIO_SERVER_PORT);
  server.send(200, "text/plain", status);
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