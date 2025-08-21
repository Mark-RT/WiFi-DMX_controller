#include <Arduino.h>
#include <ESPDMX.h>

DMXESPSerial dmx;

#define WIFI_SSID "Krop9"
#define WIFI_PASS "0964946190"

#include <GyverDBFile.h>
#include <LittleFS.h>
// база данных для хранения настроек
// будет автоматически записываться в файл при изменениях
GyverDBFile db(&LittleFS, "/data.db");

#include <SettingsGyver.h>
// указывается заголовок меню, подключается база данных
SettingsGyver sett("My Settings", &db);

// ключи для хранения в базе данных
enum kk : size_t
{
    reset_btn,
    white_sw,
    rainbow_sw,

    main_bright_sld,
    palitra1_sld,
    palitra2_sld,
    white_sld,
    rainbow_sld,

    strobe_btn,
    strobe_sld,
};

// билдер! Тут строится наше окно настроек
void build(sets::Builder &b)
{

    if (b.beginRow("", sets::DivType::Block))
    {
        if (b.Button("Reset"))
        {
            Serial.print("Reset: ");
            Serial.println(b.build.pressed());
        }
        b.Switch(kk::white_sw, "Білий");
        b.Switch(kk::rainbow_sw, "Радуга");
        b.endRow();
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println();

    // ======== WIFI ========

    // STA
    WiFi.mode(WIFI_AP_STA);
    // if (strlen(WIFI_SSID)) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint8_t tries = 20;
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        if (!--tries)
            break;
    }
    Serial.println();
    Serial.print("Connected: ");
    Serial.println(WiFi.localIP());
    // }

    // AP
    WiFi.softAP("AP ESP");
    Serial.print("AP: ");
    Serial.println(WiFi.softAPIP());

    // ======== SETTINGS ========
    sett.begin();
    sett.onBuild(build);
    sett.onUpdate(update);

    sett.onFocusChange([]()
                       {
        Serial.print("Focus: ");
        Serial.println(sett.focused()); });

    // настройки вебморды
    // sett.config.requestTout = 3000;
    // sett.config.sliderTout = 500;
    // sett.config.updateTout = 1000;
    // sett.config.theme = sets::Colors::Green;

    // ======== DATABASE ========
#ifdef ESP32
    LittleFS.begin(true);
#else
    LittleFS.begin();
#endif

    db.begin();

    // инициализация базы данных начальными значениями
    db.init(kk::reset_btn, 0);
    db.init(kk::white_sw, 0);
    db.init(kk::rainbow_sw, 0);

    db.init(kk::main_bright_sld, 60);
    db.init(kk::palitra1_sld, 800);
    db.init(kk::palitra2_sld, 50);
    db.init(kk::white_sld, 0);
    db.init(kk::rainbow_sld, 211);

    db.init(kk::strobe_btn, 0);
    db.init(kk::strobe_sld, 0);

    // db.dump(Serial);

    // часовой пояс для rtc
    setStampZone(2);
}

void loop()
{
    // тикер, вызывать в лупе
    sett.tick();
}