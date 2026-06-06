#include <TFT_eSPI.h>
#include <SPI.h>
#include "WiFi.h"
#include <Wire.h>
#include "Button2.h"
#include <esp_adc_cal.h>
#include "bmp.h"

#include "driver/rtc_io.h"

#define BUTTON_1            35
#define BUTTON_2            0

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

char buff[512];
int vref = 1100;
int btnCick = false;

void wifi_scan();

void button_init() {
    btn1.setPressedHandler([](Button2 & b) {
        btnCick = !btnCick;
    });

    btn2.setPressedHandler([](Button2 & b) {
        wifi_scan();
    });
}

void button_loop() {
    btn1.loop();
    btn2.loop();
}

void wifi_scan() {
    tft.setRotation(0);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);

    tft.drawString("Scan Network", tft.width() / 2, tft.height() / 2);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int16_t n = WiFi.scanNetworks();
    tft.fillScreen(TFT_BLACK);
    if (n == 0) {
        tft.drawString("no networks found", tft.width() / 2, tft.height() / 2);
    } else {
        tft.setTextDatum(TL_DATUM);
        tft.setCursor(0, 0);
        Serial.printf("Found %d net\n", n);
        for (int i = 0; i < n; ++i) {
            sprintf(buff, "[%d]:%s(%d)", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
            tft.println(buff);
        }
    }
    delay(5000);
    btnCick = false;
    tft.setRotation(1);
}

void setup() {
    Serial.begin(115200);
    Serial.println("Start");

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(0, 0);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);

    tft.setSwapBytes(true);

    button_init();

    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);    //Check type of calibration value used to characterize ADC
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
        vref = adc_chars.vref;
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        Serial.printf("Two Point --> coeff_a:%umV coeff_b:%umV\n", adc_chars.coeff_a, adc_chars.coeff_b);
    } else {
        Serial.println("Default Vref: 1100mV");
    }

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
}

void loop() {
    if (btnCick) tft.pushImage(0, 0,  240, 135, popcat);
    else tft.pushImage(0, 0,  240, 135, monke);

    button_loop();
}
