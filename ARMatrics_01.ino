#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>

// --- CONFIGURATION ---
const char* ssid = "Armatrics_Alliance";       //  WiFi Name
const char* password = "securepassword";    // WiFi Password
const int CS_PIN = 5;                  // Chip Select Pin

WebServer server(80);
bool sdDetected = false;

// --- HELPER: Generate File Links (Recursive) ---
// This function adds a download link for every file found to the HTML string
void addFileLinks(fs::FS &fs, const char * dirname, String &html) {
  File root = fs.open(dirname);
  if (!root || !root.isDirectory()) return;

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      // If it's a folder, dive deeper (Recursion)
      addFileLinks(fs, file.path(), html);
    } else {
      // It is a file: Create a clickable link
      String path = String(file.path());
      String name = String(file.name());
      
      // HTML for the link: <a href="/file.txt" download>Download file.txt</a><br>
      html += "<p>File: <b>" + name + "</b> ";
      html += "<a href=\"" + path + "\" download><button>DOWNLOAD</button></a>";
      html += "</p>";
    }
    file = root.openNextFile();
  }
}

// --- HELPER: Get Content Type ---
String getContentType(String filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".txt")) return "text/plain";
  return "text/plain";
}

// --- FILE READ HANDLER ---
bool handleFileRead(String path) {
  if (!sdDetected) return false;
  if (path == "/") return false; 

  String contentType = getContentType(path);
  
  if (SD.exists(path)) {
    File file = SD.open(path, FILE_READ);
    
    // 1. Print expected size
    Serial.print("Sending file: " + path);
    Serial.print(" | Expected Size: ");
    size_t fileSize = file.size();
    Serial.print(fileSize);
    
    // 2. Optimize connection
    server.client().setNoDelay(true); 

    // 3. Send and track bytes
    size_t sent = server.streamFile(file, contentType);
    file.close();
    
    // 4. Report result
    Serial.print(" | Bytes Sent: ");
    Serial.println(sent);

    if (sent != fileSize) {
      Serial.println("ERROR: Transfer incomplete! (Check Power/WiFi Signal)");
    } else {
      Serial.println("SUCCESS: Transfer complete.");
    }
    
    return true;
  }
  return false;
}

// --- ROOT PAGE HANDLER ---
// This generates the HTML page with links
void handleRoot() {
  if (!sdDetected) {
    server.send(200, "text/plain", "Hello, SD is not here");
    return;
  }

  // Start building HTML page
  String html = "<!DOCTYPE html><html><head><title>SD Card Files</title>";
  html += "<style>body{font-family: Arial; padding: 20px;} button{background-color: #4CAF50; color: white; border: none; padding: 5px 10px; cursor: pointer;}</style>";
  html += "</head><body>";
  html += "<h1>Available Files</h1>";
  html += "<hr>";

  // Automatically find files and add links
  addFileLinks(SD, "/", html);

  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(5000); // Time to open Serial Monitor

  Serial.println("\n\nSYSTEM STARTING...");
  
  // 1. Initialize SD
  if (SD.begin(CS_PIN)) {
    sdDetected = true;
    Serial.println("SD Card: SUCCESS");
  } else {
    sdDetected = false;
    Serial.println("SD Card: FAILED");
  }

  // 2. Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("GO TO THIS URL: http://");
  Serial.println(WiFi.localIP());

  // 3. Setup Routes
  // When user visits "/", show the list
  server.on("/", handleRoot);

  // For everything else, try to find the file
  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) {
      server.send(404, "text/plain", "404: File Not Found");
    }
  });

  server.begin();
}

void loop() {
  server.handleClient();
}