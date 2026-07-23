graph TD
    subgraph Power ["Power Rail (USB-C)"]
        USB["USB-C 5V"] --> XIAO_REG["XIAO 3.3V LDO"]
        XIAO_3V3["XIAO 3V3 Rail"]
        XIAO_GND["XIAO Common GND"]
    end

    subgraph XIAO ["Seeed Studio XIAO ESP32-C6"]
        GPIO18["D10 / GPIO 18 (MOSI)"]
        GPIO19["D8 / GPIO 19 (SCK)"]
        GPIO1["D1 / GPIO 1 (CS)"]
        GPIO2["D2 / GPIO 2 (DC)"]
        GPIO21["D3 / GPIO 21 (RST)"]
        GPIO22["D4 / GPIO 22 (BUSY)"]
        GPIO9["D9 / GPIO 9 (BOOT)"]
        GPIO16["D6 / GPIO 16 (BUZZER)"]
        CHIP_PU["RST / CHIP_PU"]
    end

    subgraph EINK ["WeAct 2.9 Inch E-Paper Display"]
        E_VCC["VCC (3.3V)"]
        E_GND["GND"]
        E_DIN["DIN"]
        E_CLK["CLK"]
        E_CS["CS"]
        E_DC["DC"]
        E_RST["RST"]
        E_BUSY["BUSY"]
    end

    subgraph INPUTS ["Tactile Switches & Peripherals"]
        SW_BOOT["Boot / Refresh Button"]
        SW_RST["Hardware Reset Button"]
        BUZZER["Piezo Buzzer"]
    end

    %% Power Connections
    XIAO_3V3 --> E_VCC
    XIAO_GND --> E_GND
    XIAO_GND --> SW_BOOT
    XIAO_GND --> SW_RST
    XIAO_GND --> BUZZER

    %% Display SPI Connections
    GPIO18 -->|SPI MOSI| E_DIN
    GPIO19 -->|SPI SCK| E_CLK
    GPIO1 -->|CS Signal| E_CS
    GPIO2 -->|Data/Cmd| E_DC
    GPIO21 -->|Display Reset| E_RST
    E_BUSY -->|Busy Status| GPIO22

    %% Switch & Buzzer Connections
    GPIO9 -->|Force Refresh| SW_BOOT
    CHIP_PU -->|MCU Hardware Reset| SW_RST
    GPIO16 -->|Audio Tone| BUZZER