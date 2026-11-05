#include <HijelHID_BLEKeyboard.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoOTA.h>

HijelHID_BLEKeyboard keyboard("ESP32 PIN Entry", "Sain", 100);
WebServer server(80);
Preferences prefs;

#define TRIGGER_PIN 0  // دکمه BOOT
#define LED_PIN 2       // چراغ آبی داخلی ESP32

bool bleActive = true;
unsigned long pressStartTime = 0;
bool longPressHandled = false;

void blinkLED(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayMs);
    digitalWrite(LED_PIN, LOW);
    delay(delayMs);
  }
}

String pin1, pin2, pin3, pin4, realPin;
const int MAX_PROFILES = 8;
int activeProfile = 0;
String profNames[MAX_PROFILES];
String pk(int i, const char* s) { return "p" + String(i) + "_" + s; }
bool useEnter = true;

const char* ap_ssid = "ESP32-PinConfig";
const char* ap_password = "sain6308"; // حداقل ۸ کاراکتر

void loadActivePins() {
  prefs.begin("pins", true);
  pin1 = prefs.getString(pk(activeProfile, "1").c_str(), "1111");
  pin2 = prefs.getString(pk(activeProfile, "2").c_str(), "2222");
  pin3 = prefs.getString(pk(activeProfile, "3").c_str(), "1111");
  pin4 = prefs.getString(pk(activeProfile, "4").c_str(), "2222");
  realPin = prefs.getString(pk(activeProfile, "r").c_str(), "0000");
  useEnter = prefs.getBool(pk(activeProfile, "e").c_str(), true);
  prefs.end();
}

void loadPins() {
  prefs.begin("pins", false);
  if (prefs.getInt("act", -1) == -1) {
    // انتقال تنظیمات قدیمی به پروفایل اول
    prefs.putString("p0_n", "Phone 1");
    prefs.putString("p0_1", prefs.getString("p1", "1111"));
    prefs.putString("p0_2", prefs.getString("p2", "2222"));
    prefs.putString("p0_3", prefs.getString("p3", "1111"));
    prefs.putString("p0_4", prefs.getString("p4", "2222"));
    prefs.putString("p0_r", prefs.getString("real", "0000"));
    prefs.putBool("p0_e", prefs.getBool("enter", true));
    prefs.putInt("act", 0);
  }
  activeProfile = prefs.getInt("act", 0);
  for (int i = 0; i < MAX_PROFILES; i++) {
    profNames[i] = prefs.getString(pk(i, "n").c_str(), "");
    if (profNames[i] != "") profNames[i] = prefs.getString(pk(i, "r").c_str(), "0000");
  }
  prefs.end();

  if (activeProfile < 0 || activeProfile >= MAX_PROFILES || profNames[activeProfile] == "") {
    activeProfile = 0;
    for (int i = 0; i < MAX_PROFILES; i++) {
      if (profNames[i] != "") { activeProfile = i; break; }
    }
    if (profNames[activeProfile] == "") profNames[activeProfile] = "0000";
  }
  loadActivePins();
}

void savePins(String name, String p1, String p2, String p3, String p4, String real, bool en) {
  name = real;
  name.replace("<", "");
  name.replace(">", "");
  name.replace("'", "");
  name.replace("\"", "");
  name.replace("&", "");
  name.trim();
  if (name == "") name = "0000";

  prefs.begin("pins", false);
  prefs.putString(pk(activeProfile, "n").c_str(), name);
  prefs.putString(pk(activeProfile, "1").c_str(), p1);
  prefs.putString(pk(activeProfile, "2").c_str(), p2);
  prefs.putString(pk(activeProfile, "3").c_str(), p3);
  prefs.putString(pk(activeProfile, "4").c_str(), p4);
  prefs.putString(pk(activeProfile, "r").c_str(), real);
  prefs.putBool(pk(activeProfile, "e").c_str(), en);
  prefs.putInt("act", activeProfile);
  prefs.end();

  profNames[activeProfile] = name;
  loadActivePins();
  Serial.println("Saved profile " + String(activeProfile) + " (" + name + ")");
}

void redirectHome() {
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

void handleSelect() {
  int i = server.arg("p").toInt();
  if (i >= 0 && i < MAX_PROFILES && profNames[i] != "") {
    activeProfile = i;
    prefs.begin("pins", false);
    prefs.putInt("act", i);
    prefs.end();
    loadActivePins();
  }
  redirectHome();
}

void handleNew() {
  for (int i = 0; i < MAX_PROFILES; i++) {
    if (profNames[i] == "") {
      activeProfile = i;
      profNames[i] = "0000";
      prefs.begin("pins", false);
      prefs.putString(pk(i, "n").c_str(), profNames[i]);
      prefs.putString(pk(i, "1").c_str(), "1111");
      prefs.putString(pk(i, "2").c_str(), "2222");
      prefs.putString(pk(i, "3").c_str(), "1111");
      prefs.putString(pk(i, "4").c_str(), "2222");
      prefs.putString(pk(i, "r").c_str(), "0000");
      prefs.putBool(pk(i, "e").c_str(), true);
      prefs.putInt("act", i);
      prefs.end();
      loadActivePins();
      break;
    }
  }
  redirectHome();
}

void handleDelete() {
  int count = 0;
  for (int i = 0; i < MAX_PROFILES; i++) {
    if (profNames[i] != "") count++;
  }
  if (count > 1) {
    prefs.begin("pins", false);
    prefs.remove(pk(activeProfile, "n").c_str());
    prefs.remove(pk(activeProfile, "1").c_str());
    prefs.remove(pk(activeProfile, "2").c_str());
    prefs.remove(pk(activeProfile, "3").c_str());
    prefs.remove(pk(activeProfile, "4").c_str());
    prefs.remove(pk(activeProfile, "r").c_str());
    prefs.remove(pk(activeProfile, "e").c_str());
    profNames[activeProfile] = "";
    for (int i = 0; i < MAX_PROFILES; i++) {
      if (profNames[i] != "") { activeProfile = i; break; }
    }
    prefs.putInt("act", activeProfile);
    prefs.end();
    loadActivePins();
  }
  redirectHome();
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:system-ui,-apple-system,Roboto,sans-serif;background:#0b0b0d;color:#e4e4e7;margin:0;padding:20px 16px;min-height:100vh;box-sizing:border-box;}";
  html += ".wrap{max-width:420px;margin:auto;}";
  html += ".top{display:flex;justify-content:space-between;align-items:center;margin:8px 0 18px;}";
  html += "h2{margin:0;font-size:22px;font-weight:600;color:#f4f4f5;}";
  html += ".chip{font-size:12px;padding:4px 12px;border-radius:14px;border:1px solid #3f3f46;color:#a1a1aa;}";
  html += ".chip.on{color:#4ade80;border-color:#166534;}";
  html += ".card{background:#18181b;border:1px solid #27272a;border-radius:14px;padding:16px;margin-bottom:12px;}";
  html += ".t{font-size:12px;font-weight:600;letter-spacing:.8px;color:#a1a1aa;margin-bottom:10px;}";
  html += ".g{display:grid;grid-template-columns:1fr 1fr;gap:10px;}";
  html += ".pl{font-size:12px;color:#a1a1aa;margin-bottom:6px;text-align:center;}";
  html += "input[type=text]{width:100%;padding:12px 8px;border-radius:10px;border:1px solid #3f3f46;background:#0b0b0d;color:#fafafa;font-size:18px;text-align:center;letter-spacing:3px;box-sizing:border-box;outline:none;}";
  html += "input[type=text]:focus{border-color:#fafafa;}";
  html += "input.real{font-size:24px;font-weight:600;letter-spacing:6px;}";
  html += "input.nm{font-size:16px;letter-spacing:1px;}";
  html += "select{width:100%;padding:12px;border-radius:10px;border:1px solid #3f3f46;background:#0b0b0d;color:#fafafa;font-size:16px;}";
  html += ".btn{padding:12px;border-radius:10px;border:1px solid #3f3f46;background:#0b0b0d;color:#fafafa;font-size:15px;}";
  html += ".row{display:flex;justify-content:space-between;align-items:center;font-size:15px;}";
  html += ".row input{-webkit-appearance:none;appearance:none;width:50px;height:28px;margin:0;background:#3f3f46;border-radius:14px;position:relative;transition:.25s;flex-shrink:0;}";
  html += ".row input:before{content:'';position:absolute;top:3px;left:3px;width:22px;height:22px;background:#fff;border-radius:50%;transition:.25s;}";
  html += ".row input:checked{background:#fafafa;}";
  html += ".row input:checked:before{transform:translateX(22px);background:#0b0b0d;}";
  html += "input[type=submit]{width:100%;padding:15px;background:#fafafa;border:none;border-radius:12px;color:#0b0b0d;font-weight:600;font-size:16px;}";
  html += "input[type=submit]:active{transform:scale(.98);}";
  html += "#msg{text-align:center;font-size:14px;color:#4ade80;height:20px;margin-top:12px;opacity:0;transition:opacity .3s;}#msg.show{opacity:1;}";
  String at = " autocomplete='off' autocorrect='off' autocapitalize='off' spellcheck='false' data-lpignore='true'";
  html += "</style></head><body><div class='wrap'>";
  html += "<div class='top'><h2>PIN Config</h2><span class='chip" + String(bleActive ? " on" : "") + "'>" + String(bleActive ? "BLE on" : "BLE off") + "</span></div>";
  html += "<div class='card'><div class='t'>PROFILE</div>";
  html += "<select onchange=\"location.href='/select?p='+this.value\">";
  for (int i = 0; i < MAX_PROFILES; i++) {
    if (profNames[i] != "") {
      html += "<option value='" + String(i) + "'" + String(i == activeProfile ? " selected" : "") + ">" + profNames[i] + "</option>";
    }
  }
  html += "</select><div class='g' style='margin-top:10px'>";
  html += "<button type='button' class='btn' onclick=\"location.href='/new'\">New</button>";
  html += "<button type='button' class='btn' onclick=\"if(confirm('Delete this profile?'))location.href='/delete'\">Delete</button>";
  html += "</div></div>";
  html += "<form id='f' autocomplete='off'>";
  html += "<input style='display:none' type='text' name='fakeuser'>";
  html += "<input style='display:none' type='password' name='fakepass'>";
  html += "<div class='card'><div class='t'>DECOY PINS</div><div class='g'>";
  html += "<div><div class='pl'>PIN 1</div><input type='text' name='p1' value='" + pin1 + "'" + at + "></div>";
  html += "<div><div class='pl'>PIN 2</div><input type='text' name='p2' value='" + pin2 + "'" + at + "></div>";
  html += "<div><div class='pl'>PIN 3</div><input type='text' name='p3' value='" + pin3 + "'" + at + "></div>";
  html += "<div><div class='pl'>PIN 4</div><input type='text' name='p4' value='" + pin4 + "'" + at + "></div>";
  html += "</div></div>";
  html += "<div class='card'><div class='t'>REAL PIN</div><input class='real' type='text' name='real' value='" + realPin + "'" + at + "></div>";
  html += "<div class='card row'><span>Send Enter after each PIN</span><input type='checkbox' name='enter'" + String(useEnter ? " checked" : "") + "></div>";
  html += "<input type='submit' value='Save'>";
  html += "</form><div id='msg'>Saved successfully</div></div>";
  html += "<script>";
  html += "document.getElementById('f').addEventListener('submit',function(e){";
  html += "e.preventDefault();";
  html += "var m=document.getElementById('msg');";
  html += "fetch('/save',{method:'POST',body:new URLSearchParams(new FormData(this))})";
  html += ".then(function(r){m.textContent=r.ok?'Saved successfully':'Save failed';m.style.color=r.ok?'#4ade80':'#f87171';m.classList.add('show');if(r.ok){setTimeout(function(){location.reload();},1000);}else{setTimeout(function(){m.classList.remove('show');},2500);}})";
  html += ".catch(function(){m.textContent='Save failed';m.style.color='#f87171';m.classList.add('show');});";
  html += "});";
  html += "</script></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  savePins(server.arg("name"), server.arg("p1"), server.arg("p2"), server.arg("p3"), server.arg("p4"), server.arg("real"), server.hasArg("enter"));
  server.send(200, "text/plain", "OK");
}

void enterPIN(String pin) {
  keyboard.print(pin.c_str());
  delay(300);
  if (useEnter) {
    keyboard.tap(KEY_KP_ENTER);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  loadPins();

  WiFi.softAP(ap_ssid, ap_password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/select", handleSelect);
  server.on("/new", handleNew);
  server.on("/delete", handleDelete);
  server.begin();

  ArduinoOTA.setHostname("esp32-pin");
  ArduinoOTA.begin();

  Serial.println("Starting BLE Keyboard...");
  keyboard.begin();
}

void loop() {
  server.handleClient();
  ArduinoOTA.handle();

  bool pressed = (digitalRead(TRIGGER_PIN) == LOW);

  if (pressed) {
    if (pressStartTime == 0) {
      pressStartTime = millis();
      longPressHandled = false;
    }

    unsigned long heldTime = millis() - pressStartTime;

    if (!bleActive && heldTime >= 5000 && !longPressHandled) {
      Serial.println("Restarting BLE...");
      keyboard.begin();
      bleActive = true;
      longPressHandled = true;
      blinkLED(1, 50);
    }

  } else {
    if (pressStartTime != 0) {
      unsigned long heldTime = millis() - pressStartTime;

      Serial.print("Button released. heldTime=");
      Serial.print(heldTime);
      Serial.print(" bleActive=");
      Serial.print(bleActive);
      Serial.print(" connected=");
      Serial.println(keyboard.isConnected());

      if (bleActive && heldTime < 5000 && heldTime > 30 && keyboard.isConnected()) {
        Serial.print("pin1=[" + pin1 + "] pin2=[" + pin2 + "] pin3=[" + pin3 + "] pin4=[" + pin4 + "] real=[" + realPin + "]");
        Serial.println();

        Serial.println("Entering decoy PINs...");
        enterPIN(pin1);
        delay(800);
        enterPIN(pin2);
        delay(800);
        enterPIN(pin3);
        delay(800);
        enterPIN(pin4);
        delay(800);

        Serial.println("Entering real PIN...");
        enterPIN(realPin);

        Serial.println("Turning off BLE...");
        keyboard.end();
        bleActive = false;

        blinkLED(15, 100);
      }
    }
    pressStartTime = 0;
    longPressHandled = false;
  }

  delay(20);
}
