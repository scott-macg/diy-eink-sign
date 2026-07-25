#ifndef BATTERY_CURVE_H
#define BATTERY_CURVE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <vector>

#define BATTERY_CURVE_FILE_PATH "/battery_curve.csv"

struct BatteryPoint {
    float voltage;
    int percent;
};

class BatteryCurveManager {
public:
    std::vector<BatteryPoint> points;
    bool hasCurve;

    BatteryCurveManager();
    bool begin();
    bool load();
    int getPercent(float voltage);
    void createDefaultCsv();
};

extern BatteryCurveManager batteryCurveManager;

#endif // BATTERY_CURVE_H
