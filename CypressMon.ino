#include <adafruit_feather.h>
#include <adafruit_mqtt.h>
#include <Wire.h>

/**
  Connects to AWS IoT with mutual verification.
   User enters '0' or '1' to update AWS shadow device as well as
   the LED on WICED module.
*/

/**
  WiFi password and ssid.
*/
#include "wifi.h" // Put your wireless info here and add this file to your .gitignore

#include "certificates.h"

#define LED_PIN           PA15
#define VBAT_ENABLED      1
#define VBAT_PIN          PA1
#define SCL               PB6
#define SDA               PB7
#define A1                PA1

uint8_t MAC_array[6];
char MAC_char[18];

AdafruitMQTT mqtt;

//======================================================
#define AWS_IOT_MQTT_HOST              "a3bfp50eiiclp5.iot.us-east-1.amazonaws.com"
#define AWS_IOT_MQTT_PORT              8883
#define AWS_IOT_MQTT_CLIENT_ID         "Cypress"
#define AWS_IOT_MY_THING_NAME          "Cypress"
#define AWS_IOT_MQTT_CLEAN_SESSION     true
#define AWS_IOT_MQTT_KEEPALIVE         300
//======================================================

#define AWS_IOT_MQTT_TOPIC             "$aws/things/" AWS_IOT_MY_THING_NAME "/shadow/update"

#define SHADOW_PUBLISH_STATE_OFF      "{ \"state\": {\"reported\": { \"status\": \"OFF\" } } }"
#define SHADOW_PUBLISH_STATE_ON       "{ \"state\": {\"reported\": { \"status\": \"ON\" } } }"

/**
  @brief  Disconnect handler for MQTT broker connection.
*/
void disconnect_callback(void)
{
  Serial.println();
  Serial.println("-----------------------------");
  Serial.println("DISCONNECTED FROM MQTT BROKER");
  Serial.println("-----------------------------");
  Serial.println();
}

/**
  @brief  The setup function runs once when the board comes out of reset.
*/
void setup()
{
  //  Wire.begin(8);                // join i2c bus with address #8
  //  Wire.onReceive(receiveEvent); // register event
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);

  // Uncomment to wipe stored wifi profiles.
  // Feather.clearProfiles();

  // Wait for the USB serial port to connect. Needed for native USB port only
  while (!Serial) delay(1);

  Serial.println("AWS IOT Example\r\n");

  // Print all software versions
  Feather.printVersions();

  // Print AP profile list
  Serial.println("Saved AP profile");
  Serial.println("ID SSID                 Sec");
  for (uint8_t i = 0; i < WIFI_MAX_PROFILE; i++)
  {
    char * profile_ssid = Feather.profileSSID(i);
    int32_t profile_enc = Feather.profileEncryptionType(i);

    Serial.printf("%02d ", i);
    if ( profile_ssid != NULL )
    {
      Serial.printf("%-20s ", profile_ssid);
      Feather.printEncryption( profile_enc );
      Serial.println();
    } else
    {
      Serial.println("Not Available ");
    }
  }
  Serial.println();

  // Add defined SSID to profile if not already
  if ( Feather.checkProfile(WLAN_SSID) )
  {
    Serial.printf("\"%s\" : is already in profile list\r\n", WLAN_SSID);
  }
  else
  {
    Serial.print("Adding ");
    Serial.print(WLAN_SSID);
    Serial.println(" to profile list");

    Feather.addProfile(WLAN_SSID, WLAN_PASS, ENC_TYPE);
  }

  Serial.println("Attempting to connect with saved profile");

  // Connect to Wifi network using saved profile list
  while ( !Feather.connect() )
  {
    delay(500); // delay between each attempt
  }

  // Connected: Print network info
  Feather.printNetwork();

  Feather.macAddress(MAC_array);

  for (int i = 0; i < sizeof(MAC_array); ++i) {
    sprintf(MAC_char, "%s%02x", MAC_char, MAC_array[i]);
  }

  Serial.println(MAC_char);

  // Tell the MQTT client to auto print error codes and halt on errors
  mqtt.err_actions(true, true);

  // Set ClientID
  mqtt.clientID(AWS_IOT_MQTT_CLIENT_ID);

  // Set the disconnect callback handler
  mqtt.setDisconnectCallback(disconnect_callback);

  // default RootCA include certificate to verify AWS (
  Feather.useDefaultRootCA(true);

  // Setting Identity with AWS Private Key & Certificate
  mqtt.tlsSetIdentity(aws_private_key, local_cert, LOCAL_CERT_LEN);

  // Connect with SSL/TLS
  Serial.printf("Connecting to " AWS_IOT_MQTT_HOST " port %d ... ", AWS_IOT_MQTT_PORT);
  mqtt.connectSSL(AWS_IOT_MQTT_HOST, AWS_IOT_MQTT_PORT, AWS_IOT_MQTT_CLEAN_SESSION, AWS_IOT_MQTT_KEEPALIVE);
  Serial.println("OK");

  Serial.print("Subscribing to " AWS_IOT_MQTT_TOPIC " ... ");
  mqtt.subscribe(AWS_IOT_MQTT_TOPIC, MQTT_QOS_AT_MOST_ONCE, subscribed_callback); // Will halt if an error occurs
  Serial.println("OK");

  // Message to user
  Serial.print("Enter '0' or '1' to update feed: ");
}

/**
  @brief  This loop function runs over and over again.
*/
void loop()
{
  // Get input from user. '0' is off, '1' is on.
  if ( Serial.available() )
  {
    char c = Serial.read();

    // echo
    Serial.println(c);
    Serial.println("Publishing to " AWS_IOT_MQTT_TOPIC "... ");
    Serial.println();
    if ( c == '0' )
    {
      mqtt.publish(AWS_IOT_MQTT_TOPIC, SHADOW_PUBLISH_STATE_OFF, MQTT_QOS_AT_LEAST_ONCE); // Will halt if an error occurs
    } else if ( c == '1' )
    {
      mqtt.publish(AWS_IOT_MQTT_TOPIC, SHADOW_PUBLISH_STATE_ON, MQTT_QOS_AT_LEAST_ONCE); // Will halt if an error occurs
    } else
    {
      // do nothing
    }
    Serial.print("Enter '0' or '1' to update feed: ");
  }

  delay(500);
}

// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int howMany) {
  while (1 < Wire.available()) { // loop through all but the last
    char c = Wire.read(); // receive byte as a character
    Serial.print(c);         // print the character
  }
  int x = Wire.read();    // receive byte as an integer
  Serial.println(x);         // print the integer
}

void reset() {
  delay(500);
  Feather.sdep(SDEP_CMD_RESET, 0, NULL, NULL, NULL);
}

float readVBat() {
  int ar = analogRead(VBAT_PIN);
  float vbat = (ar * 0.080566F) ;
  return vbat;
}

void onWifiDisconnected() {
  reset();
}

/**************************************************************************/
/*!
    @brief  MQTT subscribe event callback handler

    @param  topic      The topic causing this callback to fire
    @param  message    The new value associated with 'topic'

    @note   'topic' and 'message' are UTF8Strings (byte array), which means
            they are not null-terminated like C-style strings. You can
            access its data and len using .data & .len, although there is
            also a Serial.print override to handle UTF8String data types.
*/
/**************************************************************************/
void subscribed_callback(UTF8String topic, UTF8String message)
{
  if ( 0 == memcmp(SHADOW_PUBLISH_STATE_OFF, message.data, message.len) )
  {
    // Serial.println("Turning LED off.");
    digitalWrite(LED_PIN, LOW);
  }

  if ( 0 == memcmp(SHADOW_PUBLISH_STATE_ON, message.data, message.len) )
  {
    // Serial.println("Turning LED on.");
    digitalWrite(LED_PIN, HIGH);
  }
}

/**************************************************************************/
/*!
    @brief  Connect to defined Access Point
*/
/**************************************************************************/
bool connectAP(void)
{
  // Attempt to connect to an AP
  Serial.print("Please wait while connecting to: '" WLAN_SSID "' ... ");

  if ( Feather.connect(WLAN_SSID, WLAN_PASS) )
  {
    Serial.println("Connected!");
  }
  else
  {
    Serial.printf("Failed! %s (%d)", Feather.errstr(), Feather.errno());
    Serial.println();
  }
  Serial.println();

  return Feather.connected();
}
