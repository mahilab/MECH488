## myRIO Hardware Setup (MECH 488 TAs only)

- Install [LabVIEW myRIO Toolkit 2019 (32-bit)](https://www.ni.com/en-us/support/downloads/software-products/download.labview-myrio-toolkit.html#305910).
- Connect the myRIO via USB to host PC and find the device in Measurement and Automation Explorer (MAX) under `Remote Systems`.
- Ensure that the current firmware version is `7.0.0f0`. If it is not, click the `Update Firmware` button and install the latest firmware.
- Ensure `Enable Secure Shell Serve (sshd)` is enabled. 

![MAX](https://raw.githubusercontent.com/mahilab/MECH488/master/misc/max.png)

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


