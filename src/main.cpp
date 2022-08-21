#include "SPIFFS.h"
#include "WiFi.h"

#define WIFI_TIMEOUT 20000

const char *ssidPath = "/wifi-ssid.txt";
const char *passPath = "/wifi-pass.txt";

void initSPIFFS()
{
    if (!SPIFFS.begin())
    {
        ESP.restart();
    }
}

String readFile(fs::FS &fs, const char *path)
{
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if (!file || file.isDirectory())
    {
        Serial.println("- failed to open file for reading");
        return String();
    }

    String fileContent;
    while (file.available())
    {
        fileContent = file.readStringUntil('\n');
        break;
    }
    return fileContent;
}

void connectToWifi()
{
    String ssid = readFile(SPIFFS, ssidPath);
    String pass = readFile(SPIFFS, passPath);

    WiFi.mode(WIFI_STA);
    WiFi.begin(&ssid[0], &pass[0]);

    Serial.print("Connecting to WiFi");
    unsigned long startAttemptMs = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - startAttemptMs > WIFI_TIMEOUT)
        {
            Serial.print("\nConnection failed, restarting...");
            ESP.restart();
        }
        Serial.print(".");
        delay(100);
    }

    Serial.print("Connected on IP: ");
    Serial.println(WiFi.localIP());
}

void setup()
{
    Serial.begin(115200);
    initSPIFFS();
    connectToWifi();
}

void loop()
{
}