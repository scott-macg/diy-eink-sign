#include "battery_curve.h"

BatteryCurveManager batteryCurveManager;

BatteryCurveManager::BatteryCurveManager() : hasCurve(false) {}

void BatteryCurveManager::createDefaultCsv() {
    File f = LittleFS.open(BATTERY_CURVE_FILE_PATH, "w");
    if (!f) {
        Serial.println("[BATTCURVE] Failed to create default battery_curve.csv");
        return;
    }
    // Calibrated hybrid battery curve (empirical 6.8h profiling + LiPo model)
    f.println("voltage,percent");
    f.println("4.15,100");
    f.println("4.08,90");
    f.println("4.01,80");
    f.println("3.94,70");
    f.println("3.84,60");
    f.println("3.79,50");
    f.println("3.75,40");
    f.println("3.70,30");
    f.println("3.65,20");
    f.println("3.52,10");
    f.println("3.30,0");
    f.close();
    Serial.println("[BATTCURVE] Created calibrated battery_curve.csv in LittleFS");
}

bool BatteryCurveManager::begin() {
    if (!LittleFS.exists(BATTERY_CURVE_FILE_PATH)) {
        Serial.println("[BATTCURVE] battery_curve.csv not found. Creating placeholder curve...");
        createDefaultCsv();
    }
    return load();
}

bool BatteryCurveManager::load() {
    points.clear();
    hasCurve = false;

    File f = LittleFS.open(BATTERY_CURVE_FILE_PATH, "r");
    if (!f) {
        Serial.println("[BATTCURVE] Could not open battery_curve.csv for reading");
        return false;
    }

    bool isHeader = true;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        if (isHeader) {
            isHeader = false;
            // Skip CSV header line ("voltage,percent") if present; otherwise treat as data
            if (line.startsWith("voltage") || line.startsWith("Voltage") || line.startsWith("V")) {
                continue; // Confirmed header row — skip it
            }
            // No header present — fall through to parse this line as a data point
        }

        int commaIdx = line.indexOf(',');
        if (commaIdx != -1) {
            float v = line.substring(0, commaIdx).toFloat();
            int p = line.substring(commaIdx + 1).toInt();
            if (v > 0.0f) {
                points.push_back({v, p});
            }
        }
    }
    f.close();

    if (points.size() >= 2) {
        hasCurve = true;
        Serial.printf("[BATTCURVE] Loaded %d curve calibration points from battery_curve.csv\n", (int)points.size());
        return true;
    }

    Serial.println("[BATTCURVE] Insufficient points in battery_curve.csv. Falling back to linear calculation.");
    return false;
}

int BatteryCurveManager::getPercent(float voltage) {
    if (!hasCurve || points.size() < 2) {
        // Fallback linear calculation (3.3V = 0%, 4.2V = 100%)
        int pct = (int)(((voltage - 3.3f) / (4.2f - 3.3f)) * 100.0f);
        return constrain(pct, 0, 100);
    }

    // Handle out of range bounds
    if (voltage >= points.front().voltage) return constrain(points.front().percent, 0, 100);
    if (voltage <= points.back().voltage) return constrain(points.back().percent, 0, 100);

    // Piecewise linear interpolation between adjacent calibration points
    for (size_t i = 0; i < points.size() - 1; i++) {
        float v1 = points[i].voltage;
        int p1 = points[i].percent;
        float v2 = points[i + 1].voltage;
        int p2 = points[i + 1].percent;

        if ((voltage <= v1 && voltage >= v2) || (voltage >= v1 && voltage <= v2)) {
            if (abs(v1 - v2) < 0.0001f) return p1;
            float ratio = (voltage - v2) / (v1 - v2);
            int interpPct = p2 + (int)(ratio * (p1 - p2));
            return constrain(interpPct, 0, 100);
        }
    }

    return 50; // Fallback default
}
