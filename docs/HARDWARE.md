# Hardware and assembly

[Home](../README.md) · [Firmware](FIRMWARE.md) · [Setup](SETUP.md)

## Parts and revisions

- ESP32-C3 Super Mini with the same pin arrangement as the board pictured below.
- A compatible clock PCB and components selected from the matching bill of materials.
- USB power/data cable and suitable power source for the controller board.
- Optional printed diffuser, case and stand.

| Resource | Available revision/details |
| --- | --- |
| [Gerbers](../PCB%20assembly/Gerber/) | v1.5 and v1.7 |
| [BOM and placement files](../PCB%20assembly/) | Filenames identify v1.5; red and green LED BOM variants |
| [Schematic](../schematic%20v1.7.png) | v1.7 |
| [LED orientation photo](../PCB%20assembly/led%20orientation.jpg) | Check before assembly |
| [Printed parts](../STL%20files/) | Case, diffuser, combined case/diffuser and stand |

The v3.4 release introduced support for PCB v1.7. Firmware 3.6 uses the current GPIO mapping. Check that mapping and the schematic before using another board revision. The v1.5 BOM/placement files should not be assumed to match the v1.7 Gerbers without review.

<img src="https://github.com/user-attachments/assets/a2b4d4ee-3948-4200-8df7-c21813804198" alt="ESP32-C3 Super Mini pin layout used by this project" width="600">

## Assembly

1. Check the PCB revision, LED color/BOM and component orientation against the design files.
2. Mount the LEDs sideways as shown. Their orientation is essential to the charlieplexed display.
3. Solder the ESP32-C3 Super Mini to the PCB, matching its pin arrangement. Inspect for bridges before applying power.
4. Install the firmware and run **Display Test** from the setup portal. Confirm every segment and the colon illuminate.
5. Fit the printed diffuser and case after electrical checks pass.

<img src="https://github.com/user-attachments/assets/e89deed8-6aca-40ce-afee-97134708f9e2" alt="Side-mounted LEDs along the bottom of the PCB" width="480">

## Printing and mounting

White material works well for diffusion. A dark rear surface reduces light bleed; with a multicolor printer, the first four layers can be black. Avoid conductive or metallic coatings near the antenna.

The original build uses short pieces of 1.75 mm PLA filament, approximately 7 mm long, as alignment pins. Glue and trim them after fitting, or use suitable 2 mm screws and nuts where the mounting holes allow.

<img src="https://github.com/user-attachments/assets/a3d10305-ad62-412f-80c6-2a91aec4f0dc" alt="Printed diffuser and case fitted to the PCB" width="800">

<img src="https://github.com/user-attachments/assets/baa21291-d857-48c5-8fe1-7e0899a7fff9" alt="Case and PCB mounting details" width="800">
