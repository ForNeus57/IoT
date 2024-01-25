#include "BluetoothSerial.h"   // do połączenia z telefonem
#include <WiFiClientSecure.h>  // do połączenia z WiFi
#include <PubSubClient.h>      // do połączenia z MQTT
#include <string>              // do wszystkiego co wyżej
#include <bits/stdc++.h>       // do wektora do zabezpieczenia przed wysyłaniem 2 razy tego samego pakietu
#include <stack>               // do zamiany longa na string (tak trzeba to było implementować)
#include <AESLib.h>            // do bezpiecznej komunikacji z telefonem

#define photoresistorPin 34            // pin płytki z którego zczytujemu dane
#define bluetoothEOL 10                // znak End Of Line w trakcie łączenia się przez bluetooth, definiowany przez standard
#define readingSendPeriodInSeconds 20  // wyślij odczyt do aws co każde X pętli (~ sekund)

// Bezpieczeństwo połączenia:
// Bluetooth - Secure simple pairing, hashe trzymane w pamięci przeciwko wysłaniu 2 razy tego samego pakietu, szyfrowanie wiadomości
// wifi - klucze

String ssid = "";
String pwd = "";
String users[1];                  // userowie trzymani w pamięci, przypisani do płytki. Teoretycznie nie użyte
bool doReadValue = false;         // czy zczytywać dane z fotorezystora, zmieniana przez usera
int readingTimeCounter = 0;       // czuwa nad tym, czy trzeba w danej iteracji pętli wysłać odczyt do aws
long randomV = 1L;                // wartość zmieniana w każdej iteracji pętli po to, aby mogła być zhashowana i wysłana do telefonu (hashe trzymane w pamięci przeciwko wysłaniu 2 razy tego samego pakietu)
std::vector<String> hashHistory;  // historia hashy otrzymanych od telefonu


BluetoothSerial bt;                 // klient do łączenia się przez bluetooth
uint16_t lastReading = 0;           // ostatni odczyt z fotorezystora, zmienia się tylko gdy doReadValue==true
WiFiClientSecure wifiClient;        // klient do połączenia się po WiFi
String mac = "0C::B8:15:F3:79:AE";  // adres MAC urządzenia w razie gdyby był potrzebny
String deviceId = "";               // id urządzenia dostawane od telefonu, używane do wysyłania danych do AWS

const char* host = "auscx70kz24ty-ats.iot.eu-north-1.amazonaws.com";  // AWS endpoint do wysyłania odczytów

const char* certificate_pem_crt = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDWTCCAkGgAwIBAgIUCUKkVTT+rzaSbA2xvlYhOlySO1YwDQYJKoZIhvcNAQEL
BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g
SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTIzMTIyODEwMDcy
NFoXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0
ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBANSJdRvYZ1mfZiJu0V8p
FYvw/vMQfa2GnRGXkrb14vEiBV5GNkRr4ONnud1MffEti8QXFkDDPybCQTMaJ+lH
CCJTYjmBPD4Hss9CEejSuswdq2CpP/5+fBasdOyXqYG6A3TWSjEBORhCrHe8u21v
+a1IJWnxdFtEdGuSxFJLfwPtiGNXRVvXBH2SRVY4lyqiDjXQOkNFCqovBXTK60qC
xhG29WN/UbWsqKQ0qs7S/ulM9FEdGGCK7et6OJSVXmKuMbI5mCpUEfX7eQvT7/Tr
HxIMqPLyp+7F5BeMCKEysybWMZevAIm4FBTwyoZmabHaAadj8twpR6liyVy7Xvqe
2xkCAwEAAaNgMF4wHwYDVR0jBBgwFoAUHB1/I9Yt6EOwKbTSmuV/MpWL7XQwHQYD
VR0OBBYEFCZvFLE4TaKc980r5fBTuKDwYVp0MAwGA1UdEwEB/wQCMAAwDgYDVR0P
AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQALwRjvahzpC8txZh6u+OjYZaeF
ZwzPzLUV1kbvR6ayqP9hfkBZNvS8Q7KLxff5Cl/uNGom2o1zRFGL+rraqAE0csnO
cZ9T0DNbQyYZj17paqoLMR/XBIoQM9AUons4DjrwR/z8s37lCgdBAmCPqQ6aG1aa
Ea1cUrtez88dgf+8YAmOeHLLkQAYvSx9XHpLBROVbqsZmXpG6OcKPycIh9ch6ew8
cobjO0rsfZNTMCUCymnqUj1Yn+dr7jWrE8tAwlTM0IZFPsNyjJAb8epFoK2CAAf8
4mDtZnDabFKsdzsIHdwgWYa3PKjyeRLxoOXMw/FtPvnr4aU5qtIHL/UytCPa
-----END CERTIFICATE-----
)EOF";

// xxxxxxxxxx-private.pem.key
const char* private_pem_key = R"EOF(
-----BEGIN RSA PRIVATE KEY-----
MIIEowIBAAKCAQEA1Il1G9hnWZ9mIm7RXykVi/D+8xB9rYadEZeStvXi8SIFXkY2
RGvg42e53Ux98S2LxBcWQMM/JsJBMxon6UcIIlNiOYE8Pgeyz0IR6NK6zB2rYKk/
/n58Fqx07JepgboDdNZKMQE5GEKsd7y7bW/5rUglafF0W0R0a5LEUkt/A+2IY1dF
W9cEfZJFVjiXKqIONdA6Q0UKqi8FdMrrSoLGEbb1Y39RtayopDSqztL+6Uz0UR0Y
YIrt63o4lJVeYq4xsjmYKlQR9ft5C9Pv9OsfEgyo8vKn7sXkF4wIoTKzJtYxl68A
ibgUFPDKhmZpsdoBp2Py3ClHqWLJXLte+p7bGQIDAQABAoIBAGfytrEgvSNrP3O1
yXcGTAB+pOLSfPVCsk1pWZtcVVtkLUX3hxEdxgbSs2fVwn8TrJkCRaL1G0zkkyjg
MQb587N+HrtLRuB7uWt+v+2kKzIrjKchACiDSkN4o1MXthK5nHknWW/wTjFcYqfJ
Sc3zhyTF9W6EZklBCMOYK6aoP/316YRVen1OvPl03wUcCAgL4M3yNFIGiqBz5WyZ
pjC+XQCRhVZkH5A4PFA2G/60oS+JAEzWJOM/hPFiyQVtOvVac/pQLw6a2xMmZmAM
5DKlx39/HtxiY75j+Ub1Ke/rERFakLfJh/Wo//+UZGgXXmOUKRX7tKA1WxxgQKTa
WYIdLRECgYEA8SYafxxRQ7ZNtAuzFvtbdBF0yEp/uBoGhZbnf3z+MEZk6KPUBwbJ
IdMi6G8AcDCxGOrpxZH9jA/XbiXQXEwPHNFB+mbYQe/iEzUtbFs96/XAH/MIEuRF
AGj/Z++w5BUTJvQZvP+TTrRwsCxMkK2x/ckx7/cwIShMUYCVfhOOf40CgYEA4aBD
8qwZzz+1qAuqSWMF1qzX1D05/PHpgDTKtjJKYzbsDzmt6WjWobh1cMZiNV0ig+vh
KGLOtmgcAr7oJZnTJxUenNrUZXz8i57L/Gt9PvPlnWC867kk/P4R8ZlGcXwqIl1o
x483LujW5z2hQnVAqjsz9pRa26eEZ+ztn/NRcL0CgYBUz4rYE5R9FPulC8xTk9lp
K85/trvOetC6YQVP8BzRy6AntZ6XTgqk68XGK2vjCSzvz4aiJqMehF/G7GOoYlOa
Vfo3X75FCmHfUX+FMV3Iw+hrFCVNx5yA9WyGGGWFAeh3dbgroVMkhg9v1lSjOYN0
9zKp66ywrKSSsX9iFOyz7QKBgG5Yp2Kj8Ot/SSSqr9m8aDqZxeSzHQ0scvqU1x9M
cT0cu06m8vtYnr/xKt7A538Z0aubTT8nM9naCPj+zSK2nKJcneAw1ffcrNbMDw+g
slx7hCz4Bu1yWziwOa5jPCR8iU7NSFVxIZf7oD7v0VJjbgDTe5J49AbBYuxA3+Z+
ZbIxAoGBAMYYp36SBPj77nFCTqeI04dR9zcgv3f7+SNbpIzuo3UKB7huq0omBDge
42WDHc6v2UBo9HOB8HqZio1YppTWRYyqwniBXmgC58rOxrY6M1x46zdeXjBlbfsw
e5Sy2zNfE01vLhQ5Pomh6ivro7007xapy3rjm4hfPwofR89dmPnH
-----END RSA PRIVATE KEY-----
)EOF";


const char* rootCA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

void msgReceived(char* topic, byte* payload, unsigned int len){};  // metoda do odbierania danych z AWS przy wysłaniu requestu, nie użyta, potrzebna do utworzenia klienta MQTT

PubSubClient pubSubClient(host, 8883, msgReceived, wifiClient);  // klient do łączenia się z MQTT

// ta funkcja inicjalizuje płytkę za kazdym razem gdy jest włączona - narzucona przez Arduino
void setup() {
  // pinMode(photoresistorPin, ANALOG);
  Serial.begin(9600);  // do odczytnia logów z płytki
  delay(50);
  bt.enableSSP();            // Secure Simple Pairing - protokół bezpieczeństwa da bluetooth
  bt.begin("Fotorezystor");  // włącz bluetooth + nazwa urządzenia

  wifiClient.setCACert(rootCA);  // ustawianie certyfikatów do łączenia z AWS
  wifiClient.setCertificate(certificate_pem_crt);
  wifiClient.setPrivateKey(private_pem_key);
}

//ta metoda wykonuje się w kółko po zainicjalizowaniu płytki - narzucona przez Arduino
void loop() {
  maintenance();

  connectToWiFi();

  readValue();

  readBluetooth();
}

// proste akcje które muszą się wykonać co każdą iterację pętli
void maintenance() {
  wifiClient.setTimeout(15000);   // timeout dla requestów
  randomV = random(0, LONG_MAX);  // zmienna globalna hashowana przy wysłaniu odpowiedzi do telefonu

  delay(1000);

  readingTimeCounter += 1;                                  // pilnuje wysłania odczytu do AWS
  if (readingTimeCounter > readingSendPeriodInSeconds + 1)  // prevent int overflow
    readingTimeCounter = 1;

  Serial.println("Starting new loop...");
}

//Odczytuje wartość z fotorezystora
void readValue() {
  if (!doReadValue)  // nie czytaj jeśli user nie powiedział żeby czytać
    return;

  lastReading = analogRead(photoresistorPin);  //ta metoda jest domyślnie dostępna na płytkach i odczytuje napięcie z pinu
  if (lastReading != 0) {
    Serial.print("Photoresistor reading: ");
    Serial.println(std::to_string(lastReading).c_str());
  }

  sendReading();
}

// wysłanie requestu z odczytem do AWS
void sendReading() {
  if (readingTimeCounter < readingSendPeriodInSeconds)  // wyślij odczyt tylko co X pętli ~ sekund
    return;

  if (deviceId.length() == 0) {  // Wysyłamy tylko jak zdefiniowane jest id płytki
    Serial.println("\n\nERROR: Could not send reading to AWS, deviceId is null\n\n");
    return;
  }

  bt.end();  // żeby wysłać trzeba zamknąć bluetooth, inaczej nie działa
  Serial.println("Sending reading request to AWS...");

  String sensorData = "{\"device_id\":\"" + deviceId + "\",\"value\":" + lastReading + "}";  //body requestu wysyłanego do AWS

  int i = 0;
  if (!pubSubClient.connected()) {
    while (!pubSubClient.connected() && i < 8) {
      Serial.print(".");
      pubSubClient.connect("Fotorezystor");
      delay(1000);
      i++;
    }
    Serial.println(" connected");
  }
  pubSubClient.loop();

  boolean rc = pubSubClient.publish("write", sensorData.c_str());
  Serial.print("Message published, rc=");
  Serial.print((rc ? "OK: " : "FAILED: "));
  Serial.println(sensorData);

  bt.begin("Fotorezystor");  // włącz bluetooth po tym jak wyłączyłeś
}

// funkcja opakowująca wysłanie przez bluetooth
void sendBt(String msg) {
  bt.println(encrypt(msg));  // enkrypcja przez AES128, po drugiej stronie wymagane są taki sam klucz symetryczny (do odkodowania)
}

//Sprawdzanie czy ktoś chce się połączyć przez bluetooth
void readBluetooth() {
  int i = 0;
  while (!bt.available() && i < 8) {
    delay(50);
    i += 1;
  }
  if (!bt.available()) return;
  Serial.println("Bluetooth device has connected succesfully!");
  String message = "";
  char cmd;

  do {  // zczytuj wiadomość dopóki nie napotkasz znaku końca transmisji (globalna zmienna bluetoothEOL na początku pliku)
    cmd = bt.read();
    message += cmd;
  } while (cmd != bluetoothEOL);

  handleRequest(decrypt(message));
}

// Rozpoznaj rozkaz i wykonaj jeśli prawidłowy
void handleRequest(String message) {
  Serial.println("-------------- Received request:" + message);

  if (message.indexOf("ssid") > 0 && message.indexOf("password") > 0)  //rozkaz podania hasła do WiFi
    handleWiFiRequest(message);
  else if (message.indexOf("doReadValue") > 0)  //rozkaz zmiany stanu odczytywania (rozparowanie urządzenia)
    handleDoReadingRequest(message);
  else
    sendBt(String("{\"message\":\"Bad request\",\"key\":\"") + encodeToSHA(randomV) + String("\"}"));
}

// zajmuje się requestem o zapisanie hasła i ssid do połączenia się z WiFi
void handleWiFiRequest(String message) {
  //{"device_id":"us","username":"us","ssid":"ss","password":"pa","key":""}
  String s = message;

  String deviceIdTmp = s.substring(s.indexOf(":") + 2, s.indexOf(",") - 1);
  s = s.substring(s.indexOf(",") + 1, s.length());

  String user = s.substring(s.indexOf(":") + 2, s.indexOf(",") - 1);

  s = s.substring(s.indexOf(",") + 1, s.length());
  String ssidTemp = s.substring(s.indexOf(":") + 2, s.indexOf(",") - 1);

  s = s.substring(s.indexOf(",") + 1, s.length());
  String pwdTemp = s.substring(s.indexOf(":") + 2, s.indexOf(",") - 1);

  s = s.substring(s.indexOf(",") + 1, s.length());
  String key = s.substring(s.indexOf(":") + 2, s.indexOf("}") - 1);

  if (!hashIsViable(key)) {  // jeśli ta wiadomość (pakiet) została już wysłana, to nie odpowiadaj
    Serial.println("Hash already used! Detected replay attack!");
    return;
  }

  deviceId = deviceIdTmp;
  pwd = pwdTemp;
  ssid = ssidTemp;

  // sprawdź czy się połączy i odpowiednio odpowiedz telefonowi
  if (!addUser(user)) {
    Serial.println("\n\nERROR: cannot add two users.\n\n");
    sendBt(String("{\"message\":\"Device already has an owner\",\"key\":\"") + encodeToSHA(randomV) + String("\"}"));
    return;
  }

  if (!tryToConnectToWifi()) {
    Serial.println("\n\nERROR: received ssid and password are incorrect.\n\n");
    sendBt(String("{\"message\":\"Bad password\",\"key\":\"") + encodeToSHA(randomV) + String("\"}"));
    return;
  }

  doReadValue = true;  // rozpocznij odczyty i wysyłanie do AWS

  Serial.println("\n\nSuccessfully changed ssid and password\n\n");
  sendBt(String("{\"message\":\"Ok\",\"key\":\"") + encodeToSHA(randomV) + String("\"}"));
}

//zajmuje się requestem o zaprzestanie/rozpoczęcie odczytów i wysyłanie na AWS
void handleDoReadingRequest(String message) {
  //{"username":"","doReadValue":true,"key":""}
  String s = message;

  String user = s.substring(s.indexOf(":") + 2, s.indexOf(",") - 1);
  if (!addUser(user)) {
    Serial.println("\n\nERROR: cannot add two users.\n\n");
    sendBt(String("{\"message\":\"Device already has an owner\",\"key\":\"") + encodeToSHA(randomV) + String("\"}"));
    return;
  }

  s = s.substring(s.indexOf(",") + 1, s.length());
  String doReadingMessage = s.substring(s.indexOf(":") + 1, s.indexOf(","));

  s = s.substring(s.indexOf(",") + 1, s.length());
  String key = s.substring(s.indexOf(":") + 2, s.indexOf("}") - 1);

  if (!hashIsViable(key)) {
    Serial.println("Hash already used! Detected replay attack!");
    return;
  }

  if (doReadingMessage == "false") {  // rozparuj urządzenia i przestań czytać z fotorezystora
    doReadValue = false;
    deviceId = "";
    users[0] = "";
  } else if (doReadingMessage == "true")  // rozpoczęcie teoretycznie zaczynamy w momencie podania dobrego hasła, więc ten if nie powinien się nigdy wykonać, stary koncept
    doReadValue = true;
  else {
    Serial.println("Bad doReadValue request: " + doReadingMessage);
    sendBt(String("{\"message\":\"Bad request\",\"key\":\"") + encodeToSHA(randomV) + String("\"}"));
    return;
  }

  Serial.println("\n\nSuccessfully fulfilled change state request\n\n");
  sendBt(String("{\"message\":\"Ok\",\"key\":\"") + encodeToSHA(randomV) + String("\"}"));
}

// funkcja próbuje się połączyć z wifi, zwraca true jak się uda i false jak się nie uda
bool connectToWiFi() {
  WiFi.begin(ssid, pwd);
  short i = 0;
  Serial.println("wifi: " + ssid + " password: " + pwd);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED && i < 8) {
    delay(50);
    Serial.print(".");
    i += 1;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(", WiFi connected, IP address: ");
    Serial.println("IP: " + WiFi.localIP().toString());
    return true;
  }
  Serial.println("Wifi not connected!");
  return false;
}

// funkcja enkryptuje dany string dzięki AES128 (padding CBC) - użyte do bezpiecznej komunikacji z telefonem
String encrypt(String message) {
  AESLib aesLib;
  aesLib.set_paddingmode((paddingMode)0);

  const char* msg = message.c_str();
  uint16_t msgLen = message.length();
  char ciphertext[2 * 129 * 2] = { 0 };
  byte aes_key[] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
  byte aes_iv[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

  Serial.println("Calling encrypt...");

  int cipherlength = aesLib.encrypt64((byte*)msg, msgLen, ciphertext, aes_key, sizeof(aes_key), aes_iv);

  return String(ciphertext);
}

// funkcja dekryptuje dany string dzięki AES128 (padding CBC) - użyte do bezpiecznej komunikacji z telefonem
String decrypt(String msg) {
  AESLib aesLib;
  aesLib.set_paddingmode((paddingMode)0);
  int msgLen = msg.length();

  char* char_array = new char[msgLen + 5];
  strcpy(char_array, msg.c_str());

  char out[2 * 129 * 2] = { 0 };
  byte aes_key[] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
  byte aes_iv[] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };
  Serial.println("Calling decrypt...");

  int decryptLen = aesLib.decrypt64(char_array, msgLen, (byte*)out, aes_key, sizeof(aes_key), aes_iv);

  return String(out);
}

//dodaje (przypisuje) usera do płytki, tylko jeden user per płytka
bool addUser(String username) {
  if (users[0] == "") {
    users[0] = username;
    Serial.println("Added user: " + username);
    return true;
  }
  return users[0] == username;
}

//funkcja próbuje połączyć się z wifi i jak się uda to zwraca true, w przeciwnym razie zwraca false
bool tryToConnectToWifi() {
  for (int i = 0; i < 5; i++) {
    if (connectToWiFi())
      return true;
  }
  return false;
}

// kodzik SHA do wysłania do telefonu w celu zapobiegnięcia replay attack (wysłanie tego samego pakietu 2 razy)
String encodeToSHA(long valueToEncode) {
  String valueAsString = LongToString(valueToEncode);
  String msg = encrypt(valueAsString);
  return msg;
}

//dodaje klucz sha wysłany przez usera, w momencie wywołania funkcji mamy pewność, ze nie został użyty
void addExternalSHA(String sha) {
  Serial.println(String("added hash: ") + sha);
  hashHistory.push_back(sha);
}

// funkcja sprawdza czy dany klucz wysłany w requescie z telefonu został już użyty
bool hashIsUsed(String sha) {
  for (String i : hashHistory) {
    if (i == sha)
      return true;
  }
  return false;
}

// funkcja sprawdza czy dany klucz wysłany w requescie od telefonu został już użyty, jesli nie, to zostaje dodany jako zużyty, jak tak, to przerywamy dzialanie i nie odpowiadamy telefonowi
bool hashIsViable(String sha) {
  if (hashIsUsed(sha))
    return false;
  addExternalSHA(sha);
  return true;
}

//funkcja zmienia typ long na typ String (tak, trzeba to było samemu implementować)
String LongToString(long long_num) {
  std::stack<char> stringStack;
  String signValue = "";

  if (long_num < 0) {
    signValue = "-";
    long_num = -long_num;
  }

  while (long_num > 0) {
    char convertedDigit = long_num % 10 + '0';
    stringStack.push(convertedDigit);
    long_num /= 10;
  }

  String long_to_string = "";

  while (!stringStack.empty()) {
    long_to_string += stringStack.top();
    stringStack.pop();
  }

  return signValue + long_to_string;
}
