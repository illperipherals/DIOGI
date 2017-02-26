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
#include "wifi.h" // Put wireless info here and add this file to your .gitignore
#include "certificates.h"

#define LED_PIN           PA15
#define VBAT_ENABLED      1
#define VBAT_PIN          PA1
#define SCL_PIN           PB6
#define SDA_PIN           PB7
#define A1                PA1

// I2C device found at address 0x08
// I2C device found at address 0x09
// I2C device found at address 0x50
// I2C device found at address 0x51
// I2C device found at address 0x7C
#define I2C_ADDRESS       0x08

uint8_t MAC_array[6];
char MAC_char[18];

AdafruitMQTT mqtt;

#define AWS_IOT_MQTT_HOST              "a3bfp50eiiclp5.iot.us-east-1.amazonaws.com"
#define AWS_IOT_MQTT_PORT              8883
#define AWS_IOT_MQTT_CLIENT_ID         "Cypress"
#define AWS_IOT_MY_THING_NAME          "Cypress"
#define AWS_IOT_MQTT_CLEAN_SESSION     true
#define AWS_IOT_MQTT_KEEPALIVE         300

#define AWS_IOT_MQTT_TOPIC             "$aws/things/" AWS_IOT_MY_THING_NAME "/shadow/update"

#define SHADOW_PUBLISH_STATE_OFF            "{ \"state\": {\"reported\": { \"status\": \"OFF\" } } }"
#define SHADOW_PUBLISH_STATE_ON             "{ \"state\": {\"reported\": { \"status\": \"ON\" } } }"
#define SHADOW_PUBLISH_ANALOG01_VALUE       "{ \"state\": {\"reported\": { \"analog01\": } } }"

/**
  @brief  Disconnect handler for MQTT broker connection.
*/
void disconnect_callback(void)
{
  Serial.println();
  Serial.println("Disconnected from MQTT broker...");
  Serial.println();
}

/**
  @brief  Set up the board.
*/
void setup()
{
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  Wire.begin();

  // Uncomment *once* to wipe stored wifi profiles.
  // Feather.clearProfiles();

  // Needed for native USB port only.
  while (!Serial) delay(1);

  Serial.println("    _ \\    _ _|      _ \\      __|    _ _|    ");
  Serial.println("    |  |     |      (   |    (_ |      |     ");
  Serial.println("   ___/ _) ___| _) \\___/ _) \\___| _) ___| _) ");

  Feather.printVersions();

  // Print AP profiles.
  Serial.println("Saved Access Point Profile");
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

  // Add defined SSID to profile if doesn't exist.
  if ( Feather.checkProfile(WLAN_SSID) )
  {
    Serial.printf("\"%s\" : is already in profile list\r\n", WLAN_SSID);
  }
  else
  {
    Serial.print("Adding ");
    Serial.print(WLAN_SSID);
    Serial.println(" to profile list.");

    Feather.addProfile(WLAN_SSID, WLAN_PASS, ENC_TYPE);
  }

  Serial.println("Attempting to connect with saved profile");

  // Connect to wifi network using saved profile list.
  while ( !Feather.connect() )
  {
    delay(500);
  }

  // Connected: Print network info.
  Feather.printNetwork();

  Feather.macAddress(MAC_array);

  for (int i = 0; i < sizeof(MAC_array); ++i) {
    sprintf(MAC_char, "%s%02x", MAC_char, MAC_array[i]);
  }

  Serial.println(MAC_char);

  // Tell the MQTT client to auto print error codes and halt on errors.
  mqtt.err_actions(true, true);

  mqtt.clientID(AWS_IOT_MQTT_CLIENT_ID);

  // Set disconnect callback handler.
  mqtt.setDisconnectCallback(disconnect_callback);

  // Default RootCA include certificate to verify AWS.
  Feather.useDefaultRootCA(true);

  // Setting Identity with AWS Private Key & Certificate.
  mqtt.tlsSetIdentity(aws_private_key, local_cert, LOCAL_CERT_LEN);

  // Connect with SSL/TLS.
  Serial.printf("Connecting to " AWS_IOT_MQTT_HOST " port %d ... ", AWS_IOT_MQTT_PORT);
  mqtt.connectSSL(AWS_IOT_MQTT_HOST, AWS_IOT_MQTT_PORT, AWS_IOT_MQTT_CLEAN_SESSION, AWS_IOT_MQTT_KEEPALIVE);
  Serial.println("OK");

  Serial.print("Subscribing to " AWS_IOT_MQTT_TOPIC " ... ");
  mqtt.subscribe(AWS_IOT_MQTT_TOPIC, MQTT_QOS_AT_MOST_ONCE, subscribed_callback); // Will halt if an error occurs
  Serial.println("OK");

  Serial.print("Enter '0' or '1' to update feed: ");
}

void loop()
{
  // Get input from user. '0' is off, '1' is on.
  if ( Serial.available() )
  {
    char c = Serial.read();

    Serial.println(c);
    Serial.println("Publishing to " AWS_IOT_MQTT_TOPIC "... ");
    Serial.println();
    if ( c == '0' )
    {
      mqtt.publish(AWS_IOT_MQTT_TOPIC, SHADOW_PUBLISH_STATE_OFF, MQTT_QOS_AT_LEAST_ONCE); // Will halt if an error occurs.
    } else if ( c == '1' )
    {
      mqtt.publish(AWS_IOT_MQTT_TOPIC, SHADOW_PUBLISH_STATE_ON, MQTT_QOS_AT_LEAST_ONCE); // Will halt if an error occurs.
    } else
    {
      // Do nothing.
    }
    Serial.print("Enter '0' or '1' to update feed: ");
  }

  delay(500);

  Wire.beginTransmission(I2C_ADDRESS);
  int available = Wire.requestFrom(I2C_ADDRESS, (uint8_t)4);

  if (available == 4)
  {
    int receivedValue = Wire.read() << 8 | Wire.read();
    Serial.println(receivedValue);
  }
  else
  {
    Serial.print("Unexpected number of bytes received: ");
    Serial.println(available);
  }

  Wire.endTransmission();

  delay(1000);
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
    @brief  Connect to defined Access Point.
*/
/**************************************************************************/
bool connectAP(void)
{
  // Attempt to connect to an AP.
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

