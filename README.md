# PackMonitor
PackMonitor is a modular 3S–6S battery analog front end that provides isolated voltage and temperature monitoring with passive cell balancing. Multiple boards can be combined to support larger battery packs.

<p align="center">
  <img width="60%" src="https://github.com/user-attachments/assets/1f166dfd-1d67-4912-bbb5-1d050e975961" />
</p>

## Description
PackMonitor is built around the LTC6810-2 multicell battery monitor and Analog Devices' isoSPI communication interface. The board uses a 2-layer PCB design to keep manufacturing and hand assembly straightforward.

## Features
- Monitoring for 3-6S cell voltages per board
- 4 Cell temperature measurements per board
- Passive cell balancing
- Isolated isoSPI communication with master board
- Support for up to 16 addressed monitoring boards

<p  float="left">
      <img width="49%" src="https://github.com/user-attachments/assets/18f93e9a-bf36-4a9c-a885-f61750287876" width="100%" />
      <img width="49%" src="https://github.com/user-attachments/assets/009b7fa2-d0c4-49a9-aab1-0223dcf57788" width="100%" />
 </p>


PackMonitor was developed for and used on UpKarting, the electric kart project of the University of Patras.
<p  float="left">
<img width="49%" alt="IMG_20260716_172228" src="https://github.com/user-attachments/assets/13058590-e574-4417-8e1c-c156ecc73d46" />
<img width="49%" alt="image" src="https://github.com/user-attachments/assets/f70d3e1b-fd1c-462e-91d3-f2ec1510a323" />

</p>

## Project Structure
The repository includes the complete Altium design files, a PDF design overview, and a portable C driver for the LTC6810-2.
```text
PackMonitor/
|-- Altium_project/               # Altium schematic and PCB design files
|   |-- BMS_slave_v2.PrjPcb
|   |-- BMS_slave_v2.BomDoc
|   |-- BMS_slave_v2.SCHLIB
|   |-- balancing.SchDoc
|   |-- slave_schematic.SchDoc
|   |-- slave_pcb.PcbDoc
|   |-- slave_pcb.PcbLib
|   `-- PackMonitor.OutJob
|-- Firmware/                     # LTC6810-2 driver
|   |-- Ltc6810.c
|   `-- Ltc6810.h
`-- PCB_Overview.PDF              # PCB design overview
```

## License
This PCB design is licensed under CERN-OHL-P-2.0:
([Source](https://github.com/nobytesgiven/PackMonitor/blob/55c5a0733cb8097451b267faaa96fd3f9e540c49/LICENSE))
