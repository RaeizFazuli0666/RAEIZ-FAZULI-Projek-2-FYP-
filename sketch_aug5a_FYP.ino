#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <DHT.h>
#include <WebServer.h>

// =====================================================
// WIFI + TELEGRAM
// =====================================================

const char* ssid = "RaeizHensem";
const char* password = "Raeiz123";

#define BOT_TOKEN "8611443504:AAHr1RVkg8F9p9vprIBFT5zsD3oN_aKNbAc"
#define CHAT_ID "7617811431"

WiFiClientSecure securedClient;
UniversalTelegramBot bot(BOT_TOKEN, securedClient);

// =====================================================
// HTTP WEB SERVER
// =====================================================

WebServer server(80);

// =====================================================
// PIN DEFINITIONS
// =====================================================

#define MQ7_PIN 34

#define DHT_PIN 27
#define DHT_TYPE DHT22

#define RCWL_PIN 26

#define BUZZER_PIN 18


// =====================================================
// OBJECTS
// =====================================================

DHT dht(DHT_PIN, DHT_TYPE);


// =====================================================
// FUNCTION DECLARATIONS
// =====================================================

void connectWiFi();
void maintainWiFi();

void setupWebServer();
void handleRoot();
void handleData();

void calibrateMQ7();
void readMQ7();

void readDHT22();
void readRCWL();

void checkDanger();

void updateEmergencySiren();

void sendTelegramAlert();

void printSystemStatus();


// =====================================================
// SENSOR VARIABLES
// =====================================================

int mq7Value = 0;
int mq7Baseline = 0;
int mq7Difference = 0;

float temperature = 0.0;
float humidity = 0.0;

bool motionDetected = false;
bool dhtValid = false;


// =====================================================
// THRESHOLDS
// =====================================================

// MQ-7 increase compared with baseline
const int MQ7_DANGER_OFFSET = 350;

// Current demonstration threshold
const float TEMP_DANGER = 34.0;


// =====================================================
// SYSTEM STATUS
// =====================================================

bool gasDanger = false;
bool heatDanger = false;
bool systemDanger = false;

bool alertSent = false;


// =====================================================
// TIMING
// =====================================================

unsigned long previousSensorTime = 0;

const unsigned long SENSOR_INTERVAL = 2000;


// Emergency siren timing

unsigned long previousSirenTime = 0;

const unsigned long SIREN_INTERVAL = 300;

bool highSirenTone = false;


// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(1000);


  // ---------------------------
  // Sensor setup
  // ---------------------------

  pinMode(MQ7_PIN, INPUT);

  pinMode(RCWL_PIN, INPUT);

  dht.begin();


  // Passive buzzer PWM

  ledcAttach(BUZZER_PIN, 2000, 8);

  ledcWriteTone(BUZZER_PIN, 0);


  // ---------------------------
  // System title
  // ---------------------------

  Serial.println();

  Serial.println("==========================================");

  Serial.println(" SMART CHILD PRESENCE DETECTION SYSTEM");

  Serial.println("==========================================");

  Serial.println();


  // ---------------------------
  // Wi-Fi
  // ---------------------------

  connectWiFi();

  securedClient.setInsecure();


  // ---------------------------
  // HTTP Web Server
  // ---------------------------

  setupWebServer();


  // ---------------------------
  // MQ-7 Calibration
  // ---------------------------

  calibrateMQ7();


  // ---------------------------
  // Telegram online message
  // ---------------------------

  bot.sendMessage(
    CHAT_ID,
    "Smart Child Presence Detection System is ONLINE.",
    ""
  );


  Serial.println();

  Serial.println("==========================================");

  Serial.println(" LIVE WEB DASHBOARD");

  Serial.println("==========================================");

  Serial.print("Open Browser : http://");

  Serial.println(WiFi.localIP());

  Serial.println("==========================================");

  Serial.println();
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop()
{
  maintainWiFi();


  // IMPORTANT:
  // Handle HTTP browser requests continuously

  server.handleClient();


  // Read sensors every 2 seconds

  if (millis() - previousSensorTime >= SENSOR_INTERVAL)
  {
    previousSensorTime = millis();


    readMQ7();

    readDHT22();

    readRCWL();


    checkDanger();


    printSystemStatus();


    // Telegram alert

    if (systemDanger && !alertSent)
    {
      sendTelegramAlert();

      alertSent = true;
    }


    // Reset alert after system becomes safe

    if (!systemDanger)
    {
      alertSent = false;
    }
  }


  // Buzzer runs continuously
  // without blocking sensors/web

  updateEmergencySiren();
}


// =====================================================
// CONNECT WIFI
// =====================================================

void connectWiFi()
{
  WiFi.mode(WIFI_STA);

  Serial.print("Connecting to Wi-Fi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println();
  Serial.println("Wi-Fi Connected!");

  Serial.print("IP Address : ");
  Serial.println(WiFi.localIP());

  Serial.print("Gateway    : ");
  Serial.println(WiFi.gatewayIP());

  Serial.println();
}

// =====================================================
// WIFI RECONNECTION
// =====================================================

void maintainWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  Serial.println("Wi-Fi disconnected...");
  Serial.println("Trying to reconnect...");

  WiFi.disconnect();
  delay(500);

  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startAttempt < 10000
  )
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Wi-Fi Reconnected!");

    Serial.print("New IP Address : ");
    Serial.println(WiFi.localIP());

    Serial.print("Dashboard : http://");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("Wi-Fi connection failed.");
  }
}


// =====================================================
// HTTP SERVER SETUP
// =====================================================

void setupWebServer()
{
  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );


  server.on(
    "/data",
    HTTP_GET,
    handleData
  );


  server.begin();


  Serial.println(
    "HTTP Web Server Started!"
  );
}


// =====================================================
// MAIN DASHBOARD PAGE
// =====================================================

void handleRoot()
{
  String html = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>
Smart Child Safety Monitoring
</title>

<style>

body
{
  font-family: Arial, sans-serif;

  margin: 0;

  background: #f2f7fb;

  color: #263238;
}


.header
{
  background: #2878b5;

  color: white;

  padding: 20px;

  text-align: center;
}


.header h1
{
  margin: 0;

  font-size: 28px;
}


.header p
{
  margin-top: 8px;

  margin-bottom: 0;
}


.container
{
  max-width: 1000px;

  margin: auto;

  padding: 20px;
}


.statusBox
{
  text-align: center;

  padding: 20px;

  border-radius: 15px;

  margin-bottom: 20px;

  font-size: 28px;

  font-weight: bold;
}


.safe
{
  background: #d5f5e3;

  color: #117864;

  border: 2px solid #2ecc71;
}


.danger
{
  background: #fadbd8;

  color: #c0392b;

  border: 2px solid #e74c3c;
}


.grid
{
  display: grid;

  grid-template-columns:
  repeat(auto-fit, minmax(210px, 1fr));

  gap: 15px;
}


.card
{
  background: white;

  border-radius: 15px;

  padding: 20px;

  box-shadow:
  0 3px 10px rgba(0,0,0,0.08);

  text-align: center;
}


.card-title
{
  font-size: 14px;

  color: #607d8b;

  margin-bottom: 10px;
}


.value
{
  font-size: 27px;

  font-weight: bold;

  color: #2878b5;
}


.normal
{
  color: #1e8449;
}


.high
{
  color: #c0392b;
}


.footer
{
  text-align: center;

  margin-top: 25px;

  color: #78909c;

  font-size: 13px;
}


.dot
{
  display: inline-block;

  width: 10px;

  height: 10px;

  background: #2ecc71;

  border-radius: 50%;

  margin-right: 6px;
}

</style>

</head>


<body>


<div class="header">

<h1>
Smart Child Presence Detection
</h1>

<p>
Safety Alert System for Parked Vehicle
</p>

</div>


<div class="container">


<div
id="systemBox"
class="statusBox safe"
>

SYSTEM STATUS : SAFE

</div>


<div class="grid">


<div class="card">

<div class="card-title">
TEMPERATURE
</div>

<div
class="value"
id="temperature"
>
-- &deg;C
</div>

</div>


<div class="card">

<div class="card-title">
HUMIDITY
</div>

<div
class="value"
id="humidity"
>
-- %
</div>

</div>


<div class="card">

<div class="card-title">
MQ-7 READING
</div>

<div
class="value"
id="mq7"
>
--
</div>

</div>


<div class="card">

<div class="card-title">
MQ-7 BASELINE
</div>

<div
class="value"
id="baseline"
>
--
</div>

</div>


<div class="card">

<div class="card-title">
MQ-7 INCREASE
</div>

<div
class="value"
id="increase"
>
--
</div>

</div>


<div class="card">

<div class="card-title">
MOTION
</div>

<div
class="value"
id="motion"
>
--
</div>

</div>


<div class="card">

<div class="card-title">
HEAT STATUS
</div>

<div
class="value"
id="heat"
>
--
</div>

</div>


<div class="card">

<div class="card-title">
GAS STATUS
</div>

<div
class="value"
id="gas"
>
--
</div>

</div>


</div>


<div class="footer">

<span class="dot"></span>

Live Monitoring

<br><br>

Data updates automatically every 1 second

</div>


</div>


<script>

function updateData()
{

fetch('/data')

.then(response =>
  response.json()
)

.then(data =>
{

document.getElementById(
  'temperature'
).innerHTML =
  data.temperature + ' &deg;C';


document.getElementById(
  'humidity'
).innerHTML =
  data.humidity + ' %';


document.getElementById(
  'mq7'
).innerHTML =
  data.mq7;


document.getElementById(
  'baseline'
).innerHTML =
  data.baseline;


document.getElementById(
  'increase'
).innerHTML =
  data.increase;


document.getElementById(
  'motion'
).innerHTML =
  data.motion;


document.getElementById(
  'heat'
).innerHTML =
  data.heat;


document.getElementById(
  'gas'
).innerHTML =
  data.gas;


let box =
document.getElementById(
  'systemBox'
);


if (
  data.system ==
  "DANGER"
)
{

  box.innerHTML =
  "SYSTEM STATUS : DANGER";

  box.className =
  "statusBox danger";

}
else
{

  box.innerHTML =
  "SYSTEM STATUS : SAFE";

  box.className =
  "statusBox safe";

}


let heat =
document.getElementById(
  'heat'
);


if (
  data.heat ==
  "HIGH"
)
{
  heat.className =
  "value high";
}
else
{
  heat.className =
  "value normal";
}


let gas =
document.getElementById(
  'gas'
);


if (
  data.gas ==
  "HIGH"
)
{
  gas.className =
  "value high";
}
else
{
  gas.className =
  "value normal";
}


let motion =
document.getElementById(
  'motion'
);


if (
  data.motion ==
  "DETECTED"
)
{
  motion.className =
  "value high";
}
else
{
  motion.className =
  "value normal";
}


})

.catch(error =>
{

console.log(
  "Dashboard connection error"
);

});

}


setInterval(
  updateData,
  1000
);


updateData();

</script>


</body>

</html>

)rawliteral";


  server.send(
    200,
    "text/html",
    html
  );
}


// =====================================================
// HTTP LIVE SENSOR DATA
// =====================================================

void handleData()
{
  String json = "{";


  json +=
    "\"temperature\":\"";


  if (dhtValid)
  {
    json +=
      String(
        temperature,
        1
      );
  }
  else
  {
    json +=
      "ERROR";
  }


  json += "\",";


  json +=
    "\"humidity\":\"";


  if (dhtValid)
  {
    json +=
      String(
        humidity,
        1
      );
  }
  else
  {
    json +=
      "ERROR";
  }


  json += "\",";


  json +=
    "\"mq7\":" +
    String(mq7Value) +
    ",";


  json +=
    "\"baseline\":" +
    String(mq7Baseline) +
    ",";


  json +=
    "\"increase\":" +
    String(mq7Difference) +
    ",";


  json +=
    "\"motion\":\"";


  if (motionDetected)
  {
    json +=
      "DETECTED";
  }
  else
  {
    json +=
      "NO MOTION";
  }


  json += "\",";


  json +=
    "\"heat\":\"";


  if (heatDanger)
  {
    json +=
      "HIGH";
  }
  else
  {
    json +=
      "NORMAL";
  }


  json += "\",";


  json +=
    "\"gas\":\"";


  if (gasDanger)
  {
    json +=
      "HIGH";
  }
  else
  {
    json +=
      "NORMAL";
  }


  json += "\",";


  json +=
    "\"system\":\"";


  if (systemDanger)
  {
    json +=
      "DANGER";
  }
  else
  {
    json +=
      "SAFE";
  }


  json += "\"";


  json += "}";


  server.send(
    200,
    "application/json",
    json
  );
}


// =====================================================
// MQ-7 CALIBRATION
// =====================================================

void calibrateMQ7()
{
  long total = 0;


  Serial.println(
    "=========================================="
  );

  Serial.println(
    " MQ-7 WARM-UP & CALIBRATION"
  );

  Serial.println(
    "=========================================="
  );


  Serial.println(
    "Keep smoke/gas away from MQ-7."
  );

  Serial.println(
    "MQ-7 warming up..."
  );


  // Warm-up 60 seconds

  for (int i = 60; i > 0; i--)
  {
    Serial.print(
      "Warm-up : "
    );

    Serial.print(i);

    Serial.println(
      " seconds"
    );


    delay(1000);
  }


  Serial.println();

  Serial.println(
    "Calibration starting..."
  );


  // 50 readings

  for (
    int i = 1;
    i <= 50;
    i++
  )
  {

    int reading =
      analogRead(
        MQ7_PIN
      );


    total += reading;


    Serial.print(
      "Reading "
    );

    Serial.print(i);

    Serial.print(
      " : "
    );

    Serial.println(
      reading
    );


    delay(100);
  }


  mq7Baseline =
    total / 50;


  Serial.println();


  Serial.print(
    "MQ-7 Baseline : "
  );

  Serial.println(
    mq7Baseline
  );


  Serial.println(
    "Calibration Complete!"
  );

  Serial.println();
}


// =====================================================
// READ MQ-7
// =====================================================

void readMQ7()
{
  long total = 0;


  // Average 10 readings

  for (
    int i = 0;
    i < 10;
    i++
  )
  {

    total +=
      analogRead(
        MQ7_PIN
      );


    delay(10);
  }


  mq7Value =
    total / 10;


  mq7Difference =
    mq7Value -
    mq7Baseline;


  if (
    mq7Difference < 0
  )
  {
    mq7Difference = 0;
  }
}


// =====================================================
// READ DHT22
// =====================================================

void readDHT22()
{
  humidity =
    dht.readHumidity();


  temperature =
    dht.readTemperature();


  if (
    isnan(humidity) ||
    isnan(temperature)
  )
  {
    dhtValid = false;
  }
  else
  {
    dhtValid = true;
  }
}


// =====================================================
// READ RCWL-0516
// =====================================================

void readRCWL()
{
  int detectedCount = 0;

  for (int i = 0; i < 10; i++)
  {
    if (digitalRead(RCWL_PIN) == HIGH)
    {
      detectedCount++;
    }

    delay(50);
  }

  // Motion is valid only if 7 out of 10 readings are HIGH
  motionDetected = (detectedCount >= 7);
}


// =====================================================
// CHECK DANGER CONDITION
// =====================================================

void checkDanger()
{
  gasDanger =
    mq7Difference >=
    MQ7_DANGER_OFFSET;


  heatDanger =
    dhtValid &&
    temperature >=
    TEMP_DANGER;


  /*
     FINAL LOGIC

     Motion detected
     AND
     temperature OR gas dangerous
  */

  systemDanger =
    motionDetected &&
    (
      heatDanger ||
      gasDanger
    );
}


// =====================================================
// EMERGENCY SIREN
// =====================================================

void updateEmergencySiren()
{
  if (!systemDanger)
  {
    ledcWriteTone(
      BUZZER_PIN,
      0
    );

    return;
  }


  if (
    millis() -
    previousSirenTime >=
    SIREN_INTERVAL
  )
  {

    previousSirenTime =
      millis();


    highSirenTone =
      !highSirenTone;


    if (
      highSirenTone
    )
    {
      ledcWriteTone(
        BUZZER_PIN,
        2200
      );
    }
    else
    {
      ledcWriteTone(
        BUZZER_PIN,
        1200
      );
    }
  }
}


// =====================================================
// TELEGRAM ALERT
// =====================================================

void sendTelegramAlert()
{
  if (
    WiFi.status() !=
    WL_CONNECTED
  )
  {

    Serial.println(
      "Telegram failed: Wi-Fi not connected."
    );

    return;
  }


  String message = "";


  message +=
    "🚨 CHILD SAFETY ALERT 🚨\n\n";


  message +=
    "Motion : ";


  if (
    motionDetected
  )
  {
    message +=
      "DETECTED\n";
  }
  else
  {
    message +=
      "NO MOTION\n";
  }


  message +=
    "Temperature : ";


  if (
    dhtValid
  )
  {

    message +=
      String(
        temperature,
        1
      );


    message +=
      " C\n";


    message +=
      "Humidity : ";


    message +=
      String(
        humidity,
        1
      );


    message +=
      " %\n";
  }
  else
  {

    message +=
      "SENSOR ERROR\n";


    message +=
      "Humidity : SENSOR ERROR\n";
  }


  message +=
    "MQ-7 Reading : ";


  message +=
    String(
      mq7Value
    );


  message +=
    "\n";


  message +=
    "MQ-7 Increase : ";


  message +=
    String(
      mq7Difference
    );


  message +=
    "\n";


  message +=
    "\nHeat Status : ";


  if (
    heatDanger
  )
  {
    message +=
      "HIGH";
  }
  else
  {
    message +=
      "NORMAL";
  }


  message +=
    "\nGas Status : ";


  if (
    gasDanger
  )
  {
    message +=
      "HIGH";
  }
  else
  {
    message +=
      "NORMAL";
  }


  message +=
    "\n\n⚠️ SYSTEM STATUS : DANGER\n\n";


  message +=
    "Please check the parked vehicle immediately!";


  bool sent =
    bot.sendMessage(
      CHAT_ID,
      message,
      ""
    );


  if (
    sent
  )
  {

    Serial.println();

    Serial.println(
      "Telegram Alert : SENT SUCCESSFULLY"
    );

  }
  else
  {

    Serial.println();

    Serial.println(
      "Telegram Alert : FAILED"
    );

  }
}


// =====================================================
// SERIAL MONITOR DISPLAY
// =====================================================

void printSystemStatus()
{
  Serial.println();

  Serial.println(
    "=========================================="
  );

  Serial.println(
    " SMART CHILD PRESENCE DETECTION SYSTEM"
  );

  Serial.println(
    "=========================================="
  );


  if (
    dhtValid
  )
  {

    Serial.print(
      "Temperature    : "
    );

    Serial.print(
      temperature,
      1
    );

    Serial.println(
      " C"
    );


    Serial.print(
      "Humidity       : "
    );

    Serial.print(
      humidity,
      1
    );

    Serial.println(
      " %"
    );

  }
  else
  {

    Serial.println(
      "Temperature    : SENSOR ERROR"
    );


    Serial.println(
      "Humidity       : SENSOR ERROR"
    );

  }


  Serial.print(
    "MQ-7 Reading   : "
  );

  Serial.println(
    mq7Value
  );


  Serial.print(
    "MQ-7 Baseline  : "
  );

  Serial.println(
    mq7Baseline
  );


  Serial.print(
    "MQ-7 Increase  : "
  );

  Serial.println(
    mq7Difference
  );


  Serial.print(
    "Motion         : "
  );


  if (
    motionDetected
  )
  {
    Serial.println(
      "DETECTED"
    );
  }
  else
  {
    Serial.println(
      "NO MOTION"
    );
  }


  Serial.print(
    "Heat Status    : "
  );


  if (
    heatDanger
  )
  {
    Serial.println(
      "HIGH"
    );
  }
  else
  {
    Serial.println(
      "NORMAL"
    );
  }


  Serial.print(
    "Gas Status     : "
  );


  if (
    gasDanger
  )
  {
    Serial.println(
      "HIGH"
    );
  }
  else
  {
    Serial.println(
      "NORMAL"
    );
  }


  Serial.print(
    "System Status  : "
  );


  if (
    systemDanger
  )
  {
    Serial.println(
      "DANGER"
    );
  }
  else
  {
    Serial.println(
      "SAFE"
    );
  }


  Serial.println(
    "=========================================="
  );
}