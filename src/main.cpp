#include <Arduino.h>

#include <WiFiConnector.h>
bool config_error = false;

#include <AutoOTA.h>
AutoOTA ota("2.4", "https://raw.githubusercontent.com/Mark-RT/myUpdater/main/WiFiDMX3/project.json");

#include <ESPDMX.h>
DMXESPSerial dmx;

// Connect GPIO02 - TDX1 to MAX3485. D4
#define pinOut 9 // указать пин управления, где есть шим

#define wifi_led 12 // D6
#define ap_led 13   // D7

#include <GyverDBFile.h>
#include <LittleFS.h>                  // база данных для хранения настроек
GyverDBFile db(&LittleFS, "/data.db"); // будет автоматически записываться в файл при изменениях

#include <SettingsGyver.h>
SettingsGyver sett("DMX пульт", &db);

enum kk : size_t // ключи для хранения в базе данных
{
    init_btn,
    rainbow_sw,

    main_bright_sld,
    color_mode, // 0 = Color Picker, 1 = Slider
    palitra1_clr,
    palitra1_sld, // Value for slider (0-1530)
    palitra2_clr,
    palitra2_sld,
    white_sld,
    rainbow_sld,
    strobe_sld,

    dimmer3_sld,
    warm_white_sld,
    cold_white_sld,

    wifi_ssid,
    wifi_pass,
    apply,
};

unsigned long lastDMXUpdate = 0; // Переменная для хранения времени

void setDMXColor(int startChannel, uint32_t color)
{
    dmx.write(startChannel, (color >> 16) & 0xFF);    // R
    dmx.write(startChannel + 1, (color >> 8) & 0xFF); // G
    dmx.write(startChannel + 2, color & 0xFF);        // B
    // dmx.update();
}

void colorWheel(int startChannel, int color)
{
    uint8_t r1 = 0, g1 = 0, b1 = 0;
    if (color <= 255)
    { // красный макс, зелёный растёт
        r1 = 255;
        g1 = color;
        b1 = 0;
    }
    else if (color > 255 && color <= 510)
    { // зелёный макс, падает красный
        r1 = 510 - color;
        g1 = 255;
        b1 = 0;
    }
    else if (color > 510 && color <= 765)
    { // зелёный макс, растёт синий
        r1 = 0;
        g1 = 255;
        b1 = color - 510;
    }
    else if (color > 765 && color <= 1020)
    { // синий макс, падает зелёный
        r1 = 0;
        g1 = 1020 - color;
        b1 = 255;
    }
    else if (color > 1020 && color <= 1275)
    { // синий макс, растёт красный
        r1 = color - 1020;
        g1 = 0;
        b1 = 255;
    }
    else if (color > 1275 && color <= 1530)
    { // красный макс, падает синий
        r1 = 255;
        g1 = 0;
        b1 = 1530 - color;
    }

    dmx.write(startChannel, 255 - r1);
    dmx.write(startChannel + 1, 255 - g1);
    dmx.write(startChannel + 2, 255 - b1);
    // dmx.update();
}

void resetDMXChannels()
{
    for (int ch = 1; ch <= 24; ch++)
    {
        dmx.write(ch, 0);
    }
    // dmx.update();

    db.set(kk::rainbow_sw, 0);

    db.set(kk::main_bright_sld, 0);
    db.set(kk::color_mode, 0);
    db.set(kk::palitra1_clr, 0x000000);
    db.set(kk::palitra1_sld, 0);
    db.set(kk::palitra2_clr, 0x000000);
    db.set(kk::palitra2_sld, 0);
    db.set(kk::white_sld, 0);
    db.set(kk::rainbow_sld, 211);
    db.set(kk::strobe_sld, 0);

    db.set(kk::dimmer3_sld, 0);
    db.set(kk::warm_white_sld, 0);
    db.set(kk::cold_white_sld, 0);
}

void initDMXFromDB()
{
    // Main brightness
    uint8_t main_bright = db.get(kk::main_bright_sld);
    dmx.write(1, main_bright);
    dmx.write(9, main_bright);

    if (db.get(kk::color_mode) == 0)
    {
        // Palette mode
        uint32_t pal1 = db.get(kk::palitra1_clr);
        dmx.write(2, (pal1 >> 16) & 0xFF); // R
        dmx.write(3, (pal1 >> 8) & 0xFF);  // G
        dmx.write(4, pal1 & 0xFF);         // B

        uint32_t pal2 = db.get(kk::palitra2_clr);
        dmx.write(10, (pal2 >> 16) & 0xFF); // R
        dmx.write(11, (pal2 >> 8) & 0xFF);  // G
        dmx.write(12, pal2 & 0xFF);         // B
    }
    else if (db.get(kk::color_mode) == 1)
    {
        // Slider mode
        int pal1_sld = db.get(kk::palitra1_sld);
        colorWheel(2, pal1_sld);

        int pal2_sld = db.get(kk::palitra2_sld);
        colorWheel(10, pal2_sld);
    }

    // White sliders
    dmx.write(5, db.get(kk::white_sld));
    dmx.write(13, db.get(kk::white_sld));

    // Rainbow switch and slider
    if (db.get(kk::rainbow_sw))
    {
        dmx.write(7, db.get(kk::rainbow_sld));
        dmx.write(15, db.get(kk::rainbow_sld));
    }
    else
    {
        dmx.write(7, 0);
        dmx.write(15, 0);
    }

    // Strobe sliders
    dmx.write(6, db.get(kk::strobe_sld));
    dmx.write(14, db.get(kk::strobe_sld));

    dmx.write(17, db.get(kk::dimmer3_sld));
    dmx.write(18, db.get(kk::warm_white_sld));
    dmx.write(19, db.get(kk::cold_white_sld));

    // dmx.update();
}

void build(sets::Builder &b)
{
    // можно узнать, было ли действие по виджету
    /*if (b.build.isAction())
    {
        Serial.print("Set: 0x");
        Serial.print(b.build.id, HEX);
        Serial.print(" = ");
        Serial.println(b.build.value);
    }*/

    if (b.beginRow())
    {
        if (b.Button("Reset"))
        {
            // Serial.print("Reset button pressed");
            resetDMXChannels();
        }
        if (b.Button(kk::init_btn, "Init"))
        {
            // Serial.print("Init button pressed");
            initDMXFromDB();
        }
        b.Label("Версія:", ota.version());
        b.endRow();
    }

    if (b.beginGroup("Кольорові прожектори"))
    {
        b.Slider(kk::main_bright_sld, "Головна яскравість", 0, 255, 5);

        b.Select(kk::color_mode, "Вибір керування кольором:", "Палітра;Слайдер");
        if (db.get(kk::color_mode) == 0)
        {
            b.Color(kk::palitra1_clr, "1 прожектор");
            b.Color(kk::palitra2_clr, "2 прожектор");
        }
        else if (db.get(kk::color_mode) == 1)
        {
            b.Slider(kk::palitra1_sld, "1 прожектор", 0, 1530, 10);
            b.Slider(kk::palitra2_sld, "2 прожектор", 0, 1530, 10);
        }

        b.Slider(kk::white_sld, "Білий", 0, 255, 5);
        b.Switch(kk::rainbow_sw, "Радуга on/off");
        b.Slider(kk::rainbow_sld, "Радуга", 211, 255, 1);
        b.Slider(kk::strobe_sld, "Стробоскоп", 0, 255, 5);
        b.endGroup();
    }

    if (b.beginGroup("Білий прожектор"))
    {
        b.Slider(kk::dimmer3_sld, "Яскравість", 0, 255, 5);
        b.Slider(kk::warm_white_sld, "Теплий", 0, 255, 5);
        b.Slider(kk::cold_white_sld, "Холодний", 0, 255, 5);
        b.endGroup();
    }

    {
        sets::Group g(b, "WiFi");
        b.Input(kk::wifi_ssid, "SSID");
        b.Pass(kk::wifi_pass, "Password");
        if (b.Button(kk::apply, "Save & Restart"))
        {
            db.update(); // сохраняем БД не дожидаясь таймаута
            ESP.restart();
        }
    }

    switch (b.build.id)
    {
    case kk::rainbow_sw:
        // Serial.print("Радуга: ");
        // Serial.println(b.build.pressed());
        if (b.build.pressed())
        {
            dmx.write(7, db.get(kk::rainbow_sld));
            dmx.write(15, db.get(kk::rainbow_sld));
            // Serial.println("Радуга відправляю значення ");
            // dmx.update();
        }
        else
        {
            // выключаем радугу
            dmx.write(7, 0);
            dmx.write(15, 0);
            // Serial.println("Радуга виключаю");
            // dmx.update();
        }
        break;

    case kk::main_bright_sld:
        // Serial.print("Гол.яскрав: ");
        // Serial.println(b.build.value);
        dmx.write(1, b.build.value);
        dmx.write(9, b.build.value);
        // dmx.update();
        break;

    case kk::color_mode:
        // Serial.print("Color mode changed: ");
        // Serial.println(b.build.value);
        b.reload();
        if (b.build.value == 0)
        {
            // Palette mode selected, set colors from DB
            uint32_t pal1 = db.get(kk::palitra1_clr);
            setDMXColor(2, pal1);

            uint32_t pal2 = db.get(kk::palitra2_clr);
            setDMXColor(10, pal2);
        }
        else if (b.build.value == 1)
        {
            // Slider mode selected, set colors from DB
            uint32_t sld1 = db.get(kk::palitra1_sld);
            colorWheel(2, sld1);

            uint32_t sld2 = db.get(kk::palitra2_sld);
            colorWheel(10, sld2);
        }
        break;

    case kk::palitra1_clr:
        // Serial.print("Палітра 1: ");
        // Serial.println(b.build.value);
        setDMXColor(2, b.build.value);
        break;

    case kk::palitra1_sld:
        // Serial.print("Слайдер 1: ");
        // Serial.println(b.build.value);
        colorWheel(2, b.build.value);
        break;

    case kk::palitra2_clr:
        // Serial.print("Палітра 2: ");
        // Serial.println(b.build.value);
        setDMXColor(10, b.build.value);
        break;

    case kk::palitra2_sld:
        // Serial.print("Слайдер 2: ");
        // Serial.println(b.build.value);
        colorWheel(10, b.build.value);
        break;

    case kk::white_sld:
        // Serial.print("Білий: ");
        // Serial.println(b.build.value);
        dmx.write(5, b.build.value);
        dmx.write(13, b.build.value);
        // dmx.update();
        break;

    case kk::rainbow_sld:
        // Serial.print("Радуга: ");
        // Serial.println(b.build.value);
        if (db.get(kk::rainbow_sw))
        {
            dmx.write(7, b.build.value);
            dmx.write(15, b.build.value);
            //  Serial.println("Радуга рухаю значення");
            // dmx.update();
        }
        break;

    case kk::strobe_sld:
        // Serial.print("Стробоскоп: ");
        // Serial.println(b.build.value);
        dmx.write(6, b.build.value);
        dmx.write(14, b.build.value);
        // dmx.update();
        break;

    case kk::dimmer3_sld:
        // Serial.println("dimmer3");
        dmx.write(17, b.build.value);
        // dmx.update();
        break;

    case kk::warm_white_sld:
        // Serial.println("warm_white");
        dmx.write(18, b.build.value);
        // dmx.update();
        break;

    case kk::cold_white_sld:
        // Serial.println("cold_white");
        dmx.write(19, b.build.value);
        // dmx.update();
        break;

        /*case 0xB: // Назва WiFi мережі button
        Serial.print("Введено назву: ");
        Serial.println(b.build.value);
        break;

        case 0xC: // WiFi password button
        Serial.print("Введено пароль: ");
        Serial.println(b.build.value);
        break;*/
    }
}

void checkUpdate()
{
    String ver, notes;
    if (ota.checkUpdate(&ver, &notes))
    {
        /*Serial.println("Знайдено оновлення!");
        Serial.println(ver);
        Serial.println(notes);*/
        ota.updateNow(); // Запускаем процесс
    }
}

void blink_tick()
{
    if (WiFiConnector.connecting())
    {
        digitalWrite(wifi_led, false);
        digitalWrite(ap_led, (millis() / 500) % 2);
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println();

    dmx.init(24);

    pinMode(wifi_led, OUTPUT);
    pinMode(ap_led, OUTPUT);
    digitalWrite(ap_led, LOW);

    // ======== DATABASE ========
#ifdef ESP32
    LittleFS.begin(true);
#else
    LittleFS.begin();
#endif

    db.begin();

    // инициализация базы данных начальными значениями
    db.init(kk::init_btn, 0);
    db.init(kk::rainbow_sw, 0);

    db.init(kk::main_bright_sld, 60);
    db.init(kk::color_mode, 0);
    db.init(kk::palitra1_clr, 0x000000);
    db.init(kk::palitra1_sld, 0);
    db.init(kk::palitra2_clr, 0x000000);
    db.init(kk::palitra2_sld, 0);
    db.init(kk::white_sld, 0);
    db.init(kk::rainbow_sld, 211);
    db.init(kk::strobe_sld, 0);

    db.init(kk::dimmer3_sld, 60);
    db.init(kk::warm_white_sld, 0);
    db.init(kk::cold_white_sld, 0);

    db.init(kk::wifi_ssid, "");
    db.init(kk::wifi_pass, "");

    setStampZone(2);

    // ======== WIFI ========
    WiFiConnector.onConnect([]()
                            {
        Serial.print("Connected. Local IP: ");
        Serial.println(WiFi.localIP()); 
    digitalWrite(wifi_led, 1);
    digitalWrite(ap_led, 0); });

    WiFiConnector.onError([]()
                          {
        Serial.print("WiFi error. AP IP: ");
        Serial.println(WiFi.softAPIP()); 
    digitalWrite(wifi_led, 0);
    digitalWrite(ap_led, 1); });

    WiFiConnector.setName("WiFi-DMX_Controller");
    WiFiConnector.setPass("12345678");
    WiFiConnector.setTimeout(40);
    WiFiConnector.connect((db[kk::wifi_ssid]), (db[kk::wifi_pass]));

    // ======== SETTINGS ========
    sett.begin();
    sett.onBuild(build);
    sett.config.theme = sets::Colors::Mint;

    initDMXFromDB();
}

void loop()
{
    WiFiConnector.tick();
    ota.tick();
    blink_tick();

    static unsigned long ota_timer = 0;
    if (millis() - ota_timer > 150000)
    {
        ota_timer = millis();
        checkUpdate();
    }

    sett.tick();

    if (millis() - lastDMXUpdate >= 100)
    {
        lastDMXUpdate = millis(); // Запоминаем текущее время
        dmx.update();             // Отправляем DMX-пакет
    }
}