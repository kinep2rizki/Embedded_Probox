#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>

//masukan pin sda(rfid pada D4) // reset pada(D3)// untuk 8266
#define SS_PIN D8
#define RST_PIN D0

// Firebase project settings
#define FIREBASE_HOST "attendance-c4ae9-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "snhgL1HtSmkqLuDdlthduF6gZk3BAT9OMhLWMtfG"

// Wi-Fi credentials
const char* ssid = "kinep2";
const char* password = "13042004";

//instance Rfid
MFRC522 rfid(SS_PIN, RST_PIN); 

// Initialize LCD and Firebase
LiquidCrystal_I2C lcd(0x27, 16, 2); // Address of the LCD, number of columns and rows
FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

void setup() {
  Serial.begin(115200); // Start the Serial communication
  while (!Serial);

  Wire.begin();         // Start I2C

  // Initialize RFID
  SPI.begin(); 
  rfid.PCD_Init(); 

  // Initialize the LCD
  lcd.begin(16, 2); // Specify number of columns and rows
  lcd.backlight();   // Turn on the backlight

  // Display IP Address
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP Address: ");
  lcd.setCursor(0, 1); // Move to the second line
  lcd.print(WiFi.localIP());
  delay(2000); // Delay for readability

  // Configure Firebase
  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;

  // Initialize Firebase
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);
  lcd.print("Firebase Initialized");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready to scan");

  // Setup time for timestamping (Set to GMT+7 for WIB)
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

void loop() {
  // Auto-reconnect to WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  // Look for new RFID tags
  if (rfid.PICC_IsNewCardPresent()) {
    if (rfid.PICC_ReadCardSerial()) {
      String rfidData = "";

      // Read RFID card ID
      for (byte i = 0; i < rfid.uid.size; i++) {
        rfidData += String(rfid.uid.uidByte[i], HEX);
      }

      // Display the RFID card ID on LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Card read: ");
      lcd.setCursor(0, 1);
      lcd.print(rfidData);

      Serial.print("RFID Card ID: ");
      Serial.println(rfidData);

      // Check the name corresponding to the RFID UID in Firebase
      checkNameInFirebase(rfidData);

      // Halt the RFID reader
      rfid.PICC_HaltA();

      // Show confirmation on LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Uploading...");
    }
  }
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
  } else {
    Serial.println("Failed to connect to WiFi. Please check credentials.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi failed");
  }

  delay(2000); // Wait 2 seconds to show WiFi status
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready to scan");
}

void checkNameInFirebase(String rfidData) {
  // Convert RFID data to uppercase to match Firebase keys
  rfidData.toUpperCase();
  String path = "/names/" + rfidData;  // Path to the UID in Firebase

  // Fetch the name from Firebase
  if (Firebase.getString(firebaseData, path)) {
    if (firebaseData.dataType() == "string") {
      String name = firebaseData.stringData();
      Serial.println("UID matched with name: " + name);

      // Display the name on the LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Name Found:");
      lcd.setCursor(0, 1);
      lcd.print(name);

      // Optional: Upload data to Firebase
      uploadToFirebase(rfidData, name);
    } else {
      Serial.println("RFID UID not found in Firebase.");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("UID not found!");
    }
  } else {
    Serial.println("Error fetching data from Firebase: " + firebaseData.errorReason());
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error fetching!");
  }

  delay(2000); // Wait 2 seconds before going back to ready state
}

void uploadToFirebase(String rfidData, String name) {
  String timestamp = getFormattedTime();
  String path = "/attendance/" + rfidData + "/" + timestamp;

  // Check if previous data is older than 24 hours and reset if necessary
  resetIfOlderThan24Hours(rfidData);

  if (Firebase.setString(firebaseData, path, name)) {
    Serial.println("Data uploaded to Firebase: " + name);
    
    // Show success on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Uploaded:");
    lcd.setCursor(0, 1);
    lcd.print(name);
  } else {
    Serial.println("Error uploading data to Firebase: " + firebaseData.errorReason());

    // Show error on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Upload failed!");
    lcd.setCursor(0, 1);
    lcd.print(firebaseData.errorReason());
  }

  delay(2000); // Wait 2 seconds before going back to ready state
}

String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  char timeStr[20];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);

  return String(timeStr);
}

void resetIfOlderThan24Hours(String rfidData) {
  String path = "/attendance/" + rfidData;

  // Get last upload timestamp
  if (Firebase.getString(firebaseData, path + "/lastUploadTime")) {
    String lastUploadTime = firebaseData.stringData();

    // Convert last upload time and current time to time_t for comparison
    struct tm lastUpload;
    struct tm currentTime;

    strptime(lastUploadTime.c_str(), "%Y-%m-%d %H:%M:%S", &lastUpload);
    time_t lastUploadEpoch = mktime(&lastUpload);

    time_t now = time(nullptr);
    localtime_r(&now, &currentTime);
    time_t currentEpoch = mktime(&currentTime);

    // Check if the difference is more than 24 hours
    double timeDiff = difftime(currentEpoch, lastUploadEpoch);

    if (timeDiff > 86400) {  // 86400 seconds = 24 hours
      // Reset the data if older than 24 hours
      if (Firebase.deleteNode(firebaseData, path)) {
        Serial.println("Old data reset in Firebase");
      } else {
        Serial.println("Failed to reset old data: " + firebaseData.errorReason());
      }
    }
  }
}
