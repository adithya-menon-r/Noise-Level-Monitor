# Noise Level Monitor

Constant exposure to high levels of noise in the workplace is proven to cause long term hearing damage. The problem is that people often don’t realise when they are being exposed to dangerous levels.

To fix this, we built a real time noise level monitor. Companies can deploy these low cost devices across their offices to continuously track noise levels and take action whenever they exceed healthy limits.

We built it using the **STM32F401 microcontroller**, and it supports:

- **Live dB Reading**: Continuously reads the analog sound sensor, converts the raw 12-bit ADC value to a calibrated decibel value, and refreshes the OLED display with live data every 50 ms.
- **Min/Max Tracking**: Keeps a running record of the peak and floor dB values seen since reset.
- **Sound Dosimetry**: Tracks how long the environment has spent in each noise category:
  - **Low**: below 60 dB
  - **Medium**: 60 - 90 dB
  - **High**: above 90 dB
- **4 mode display**: A single push-button cycles the OLED display to show the Current, Maximum, Minimum & Category Stats.
- **Threshold LED**: The onboard `PC13` LED lights up whenever the raw ADC value exceeds a set threshold.


## Getting Started
 
### Hardware
 
| Component       | Part                        | Connections                                    |
| --------------- | --------------------------- | ---------------------------------------------- |
| Microcontroller | STM32F401 Black Pill        | NA                                             |
| Sound sensor    | KY-308 (Big Sound Sensor)   | AO to PA1, GND to GND, +ve to 3V3              |
| Display         | 0.96" SSD1306 OLED (I2C)    | SCL to PB6, SDA to PB7, GND to GND, VCC to 3V3 |
| Button          | Onboard tactile push button | PA0 to GND (internal pull-up)                  |
| LED             | Onboard LED (active-low)    | PC13                                           |

### Connection Diagram
 
![Wiring and Connections](/images/wiring-and-connection.png)
 
### Building and Flashing (ARM Keil MDK)
 
1. Open the project in **Keil µVision**.
2. Go to **Project**, then **Options for Target** and confirm the device is set to `STM32F401CCUx`.
3. Under the **C/C++** tab, ensure its set to the latest version.
4. Connect the STM32F401 Black Pill to your device via **ST-Link**.
5. Click **Build** and then **Download**. This builds the code and flashes it on to the microcontroller.
7. Press the onboard reset button. The system starts up and begins reading noise levels, which are displayed on the OLED.

### Calibration
 
If readings seem off, adjust the three constants at the top of `ADC_to_dB()`:
 
```c
#define BASELINE_ADC  792.0f   // ADC reading in a quiet room
#define REFERENCE_DB   40.0f   // Known dB level at baseline
#define SENSITIVITY    20.0f   // dB change per decade of ADC delta
```
 
Measure the ambient noise with a reference meter, note the raw ADC value printed on the OLED, and update `BASELINE_ADC` and `REFERENCE_DB` accordingly.


## Contributors

| Name              | GitHub ID                                                 |
| ----------------- | --------------------------------------------------------- |
| Adithya Menon R   | [adithya-menon-r](https://github.com/adithya-menon-r)     |
| PG Karthikeyan    | [cootot](https://github.com/cootot)                       |
| Narain BK         | [NarainBK](https://github.com/NarainBK)                   |
| Anurup R Krishnan | [Anurup-R-Krishnan](https://github.com/Anurup-R-Krishnan) |

## License

This project is licensed under the [MIT LICENSE](LICENSE).
