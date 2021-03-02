# Pinout and Connections

If you plan to use the `IPendulum` interface, you are expected to connect the myRIO to the Pendulum as follows:

|Pendulum|myRIO|Comments|
|---|---|---|
|Z|-|no connection|
|B|DIO2||
|A|DIO0||
|5V|5V||
|GND|AGND/DGND|tie myRIO grounds
|ENB|DIO1|
|SNS|AI0+|
|REF+|AO0|
|REF-|AGND|
|M1|5V||
|M2|AI1+||
|M3|AGND||
|-|AI0-/AI1-/AGND|tie

# Initial Setup (MECH 488 TAs only)

## myRIO Hardware

- Install [LabVIEW myRIO Toolkit 2019 (32-bit)](https://www.ni.com/en-us/support/downloads/software-products/download.labview-myrio-toolkit.html#305910).
- Connect the myRIO via USB to host PC and find the device in Measurement and Automation Explorer (MAX) under `Remote Systems`.
- Ensure that the current firmware version is `7.0.0f0`. If it is not, click the `Update Firmware` button and install the latest firmware.
- Ensure `Enable Secure Shell Serve (sshd)` is enabled. 

![MAX](https://raw.githubusercontent.com/mahilab/MECH488/master/docs/max.png)

- SFTP the FPGA bitfile from [mahi-daq/misc](https://github.com/mahilab/mahi-daq/tree/master/misc) to the myRIO device:
```shell
cd mahi-daq/misc
sftp admin@172.22.11.2
cd /var/local/natinst
mkdir bitfiles
cd bitfiles
put NiFpga_MyRio1900Fpga60.lvbitx
chmod 777 NiFpga_MyRio1900Fpga60.lvbitx
```

## Cross-Compiler

- Download and install the [NI ARM cross-compiler](http://www.ni.com/download/labview-real-time-module-2018/7813/en/) to `C:/dev/nilrt-arm` so that the directory contains `sysroots/`, `relocate_sdk.py`, etc. 
- Once this is installed, you should be able to use the [nilrt-arm] kit with CMake Tools in VSCode to compile for the myRIO from Windows.
- Otherwise, `mahi-daq` ships with a CMake toolchain file, which can be used like so:

```shell
> cd mahi-daq
> mkdir build
> cmake .. -G Ninja -DCMAKE_TOOLCHAIN_FILE="../cmake/nilrt-arm-toolchain.cmake" -DCMAKE_BUILD_TYPE="Release"
> cmake --build .
```

## Firewall

- If you notice that the pendulum GUI is not receiving packets from the myRIO, you may need to update Windows firewall settings. For every instance of `pendulum-gui`, allow all settings as shown below:

![MAX](https://raw.githubusercontent.com/mahilab/MECH488/master/docs/firewal.png)
