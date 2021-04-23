CS4222 Group 6

1. Place the entire 'source-code' folder in (contiki/examples)

2. Open a terminal and run the following command to compile the .c file:
"make TARGET=srf06-cc26xx BOARD=sensortag/cc2650 final_proj.bin CPU_FAMILY=cc26xx"

3. In the same directory, run the following command to create the cooja simulation file:
"make TARGET=sky final_proj.upload"

4. In Uniflash, select the "CC2650F128 On-Chip Board" with the "XDS110 USB Debug Probe" then Start the session

5. Under Program, load the 'final_proj.bin' in the project directory as compiled in Step 2 to flash the program onto the SensorTag

6. To run the program on the terminal, go to the directory where contiki is located and run the following command:
"contiki/tools/sky/serialdump-linux -b115200 /dev/ttyACM0" or
"contiki/tools/sky/serialdump-linux -b115200 /dev/ttyACM1"