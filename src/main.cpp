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
    palitra1_clr,
    palitra2_clr,
    white_sld,
    rainbow_sld,
    strobe_sld,
};

void setDMXColor(int startChannel, uint32_t color) {
  dmx.write(startChannel,     (color >> 16) & 0xFF);  // R
  dmx.write(startChannel + 1, (color >> 8)  & 0xFF);  // G
  dmx.write(startChannel + 2,  color        & 0xFF);  // B
  dmx.update();
}

// билдер! Тут строится наше окно настроек
void build(sets::Builder &b) {
     // можно узнать, было ли действие по виджету
    if (b.build.isAction()) {
        Serial.print("Set: 0x");
        Serial.print(b.build.id, HEX);
        Serial.print(" = ");
        Serial.println(b.build.value);

        switch(b.build.id) {
            case 0xFFFFFFFE:
                Serial.print("Reset: ");
                Serial.println(b.build.pressed());
                break;

            case 0x1:
                Serial.print("Білий: ");
                Serial.println(b.build.pressed());
                break;

            case 0x2:
                Serial.print("Радуга: ");
                Serial.println(b.build.pressed());
                break;

            case 0x3:
                Serial.print("Гол.яскрав: ");
                Serial.println(b.build.value);
                break;

            case 0x4:
                Serial.print("Палітра 1: ");
                Serial.println(b.build.value);
                break;

            case 0x5:
                Serial.print("Палітра 2: ");
                Serial.println(b.build.value);
                break;

            case 0x6:
                Serial.print("Білий: ");
                Serial.println(b.build.value);
                break;

            case 0x7:
                Serial.print("Радуга: ");
                Serial.println(b.build.value);
                break;

            case 0x8:
                Serial.print("Стробоскоп: ");
                Serial.println(b.build.value);
                break;
        }
    }

    if (b.beginRow("", sets::DivType::Block))
    {
        b.Button("Reset");
        b.Switch(kk::white_sw, "Білий");
        b.Switch(kk::rainbow_sw, "Радуга");
        //Serial.println(db[kk::white_sw]);
        //Serial.println(db[kk::rainbow_sw]);
        b.endRow();
    }

    if (b.beginRow()) {
        b.Slider(kk::main_bright_sld, "Головна яскравість", 0, 255, 1);
        b.endRow();
    }

    if (b.beginRow()) {
        b.Color(kk::palitra1_clr, "Палітра 1");
        b.endRow();
    }

    if (b.beginRow()) {
        b.Color(kk::palitra2_clr, "Палітра 2");
        b.endRow();
    }

    if (b.beginRow()) {
        b.Slider(kk::white_sld, "Білий", 0, 255, 1);
        b.endRow();
    }

    if (b.beginRow()) {
        b.Slider(kk::rainbow_sld, "Радуга", 211, 255, 1);
        b.endRow();
    }
    if (b.beginRow()) {
        b.Slider(kk::strobe_sld, "Стробоскоп", 0, 255, 5);
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
    //sett.onUpdate(update);

    sett.onFocusChange([]()
                       {
        Serial.print("Focus: ");
        Serial.println(sett.focused()); });

    // настройки вебморды
    // sett.config.requestTout = 3000;
    // sett.config.sliderTout = 500;
    // sett.config.updateTout = 1000;
    sett.config.theme = sets::Colors::Black;

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
    db.init(kk::palitra1_clr, 0xff0000);
    db.init(kk::palitra2_clr, 0xff0000);
    db.init(kk::white_sld, 0);
    db.init(kk::rainbow_sld, 211);

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