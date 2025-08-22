#include <Arduino.h>
#include <ESPDMX.h>

DMXESPSerial dmx;

// Connect GPIO02 - TDX1 to MAX3485. D4
#define pinOut 9 // указать пин управления, где есть шим

#define wifi_led 12 // D6
#define ap_led 13   // D7

#include <GyverDBFile.h>
#include <LittleFS.h>
// база данных для хранения настроек
// будет автоматически записываться в файл при изменениях
GyverDBFile db(&LittleFS, "/data.db");

#include <SettingsGyver.h>
// указывается заголовок меню, подключается база данных
SettingsGyver sett("My DMX", &db);

// ключи для хранения в базе данных
enum kk : size_t
{
    reset_btn,
    rainbow_sw,

    main_bright_sld,
    palitra1_clr,
    palitra2_clr,
    white_sld,
    rainbow_sld,
    strobe_sld,

    wifi_ssid,
    wifi_pass,
    apply,
};

void setDMXColor(int startChannel, uint32_t color)
{
    dmx.write(startChannel, (color >> 16) & 0xFF);    // R
    dmx.write(startChannel + 1, (color >> 8) & 0xFF); // G
    dmx.write(startChannel + 2, color & 0xFF);        // B
    dmx.update();
}

void resetDMXChannels()
{
    for (int ch = 1; ch <= 16; ch++)
    {
        dmx.write(ch, 0);
    }
    dmx.update();

    db.set(kk::reset_btn, 0);
    db.set(kk::rainbow_sw, 0);

    db.set(kk::main_bright_sld, 0);
    db.set(kk::palitra1_clr, 0x000000);
    db.set(kk::palitra2_clr, 0x000000);
    db.set(kk::white_sld, 0);
    db.set(kk::rainbow_sld, 211);
    db.set(kk::strobe_sld, 0);
}

// билдер! Тут строится наше окно настроек
void build(sets::Builder &b)
{
    // можно узнать, было ли действие по виджету
    if (b.build.isAction())
    {
        Serial.print("Set: 0x");
        Serial.print(b.build.id, HEX);
        Serial.print(" = ");
        Serial.println(b.build.value);

        switch (b.build.id)
        {
        case 0xFFFFFFFE: // Reset button
            Serial.print("Reset button pressed");
            resetDMXChannels();
            break;

        case 0x1: // Rainbow switch
            Serial.print("Радуга: ");
            Serial.println(b.build.pressed());
            if (b.build.pressed())
            {
                dmx.write(7, db.get(kk::rainbow_sld));
                dmx.write(15, db.get(kk::rainbow_sld));
                Serial.println("Радуга відправляю значення ");
                dmx.update();
            }
            else
            {
                // выключаем радугу
                dmx.write(7, 0);
                dmx.write(15, 0);
                Serial.println("Радуга виключаю");
                dmx.update();
            }
            break;

        case 0x2: // Main brightness slider
            Serial.print("Гол.яскрав: ");
            Serial.println(b.build.value);
            dmx.write(1, b.build.value);
            dmx.write(9, b.build.value);
            dmx.update();
            break;

        case 0x3: // Palette 1 color
            Serial.print("Палітра 1: ");
            Serial.println(b.build.value);
            setDMXColor(2, b.build.value);
            break;

        case 0x4: // Palette 2 color
            Serial.print("Палітра 2: ");
            Serial.println(b.build.value);
            setDMXColor(10, b.build.value);
            break;

        case 0x5: // White slider
            Serial.print("Білий: ");
            Serial.println(b.build.value);
            dmx.write(5, b.build.value);
            dmx.write(13, b.build.value);
            dmx.update();
            break;

        case 0x6: // Rainbow slider
            Serial.print("Радуга: ");
            Serial.println(b.build.value);
            if (db.get(kk::rainbow_sw))
            {
                dmx.write(7, b.build.value);
                dmx.write(15, b.build.value);
                Serial.println("Радуга рухаю значення");
                dmx.update();
            }
            break;

        case 0x7: // Strobe slider
            Serial.print("Стробоскоп: ");
            Serial.println(b.build.value);
            dmx.write(6, b.build.value);
            dmx.write(14, b.build.value);
            dmx.update();
            break;

        case 0x8: // Назва WiFi мережі button
            Serial.print("Введено назву: ");
            Serial.println(b.build.value);
            break;

        case 0x9: // WiFi password button
            Serial.print("Введено пароль: ");
            Serial.println(b.build.value);
            break;
        }
    }

    if (b.beginRow("", sets::DivType::Block))
    {
        b.Button("Reset");
        b.Switch(kk::rainbow_sw, "Радуга");
        b.endRow();
    }

    if (b.beginGroup(""))
    {
        b.Slider(kk::main_bright_sld, "Головна яскравість", 0, 255, 1);
        b.Color(kk::palitra1_clr, "Палітра 1");
        b.Color(kk::palitra2_clr, "Палітра 2");
        b.Slider(kk::white_sld, "Білий", 0, 255, 1);
        b.Slider(kk::rainbow_sld, "Радуга", 211, 255, 1);
        b.Slider(kk::strobe_sld, "Стробоскоп", 0, 255, 5);
        b.endGroup();
    }

    {
        sets::Group g(b, "WiFi");
        b.Input(kk::wifi_ssid, "SSID");
        b.Pass(kk::wifi_pass, "Password");
        if (b.Button(kk::apply, "Save & Restart")) {
            db.update();  // сохраняем БД не дожидаясь таймаута
            ESP.restart();
        }
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println();
    pinMode(wifi_led, OUTPUT);
    pinMode(ap_led, OUTPUT);
    digitalWrite(ap_led, LOW);

    // ======== WIFI ========
    // STA
    WiFi.mode(WIFI_AP_STA);

     // ======== SETTINGS ========
    sett.begin();
    sett.onBuild(build);
    // sett.onUpdate(update);

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
    db.init(kk::rainbow_sw, 0);

    db.init(kk::main_bright_sld, 60);
    db.init(kk::palitra1_clr, 0x000000);
    db.init(kk::palitra2_clr, 0x000000);
    db.init(kk::white_sld, 0);
    db.init(kk::rainbow_sld, 211);
    db.init(kk::strobe_sld, 0);

    db.init(kk::wifi_ssid, "");
    db.init(kk::wifi_pass, "");

    // db.dump(Serial);

    // часовой пояс для rtc
    setStampZone(2);

    // ======= STA =======
    // если логин задан - подключаемся
    if (db[kk::wifi_ssid].length()) {
        WiFi.begin(db[kk::wifi_ssid], db[kk::wifi_pass]);
        Serial.print("Connect STA");
        int tries = 20;
        while (WiFi.status() != WL_CONNECTED) {
            delay(100);
            digitalWrite(wifi_led, LOW);
            delay(400);
            digitalWrite(wifi_led, HIGH);
            Serial.print('.');
            if (!--tries) break;
        }
        Serial.println();
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        // AP
        WiFi.softAP("DMX WiFi controller", "qwerty123");
        Serial.print("AP: ");
        Serial.println(WiFi.softAPIP());
        digitalWrite(ap_led, HIGH);
        digitalWrite(wifi_led, LOW);
    }
}

void loop()
{
    // тикер, вызывать в лупе
    sett.tick();
}