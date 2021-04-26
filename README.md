# SensorTag Contact Tracing Application

## Project aim
This project aims to build a tracing application running on a TI CC2650 SensorTag, similar to the TraceTogether token released by the Singapore government.
Our tracing device would make use of the SensorTag’s IEEE 802.15.4 (ZigBee) radio.

Our application is able to detect two devices in close proximity (defined as being within 3 meters of each other).
Through its print output, users will be able to see when a nearby device is detected, when a nearby device moves away, and an estimated duration of contact.
Our device is also able to constantly keep track of how many devices are within close proximity. This will be done while ensuring power consumption is minimised.

## Technology used
* TI CC2650 SensorTag
* ContikiOS
* Cooja simulator for power measurements

## Instructions
To run our application on a SensorTag, follow the [README](./source-code/README.txt) placed in the source-code directory.

## The team
This application was written by Davindran, Jun Wei and Shanon, a team of 3 from NUS Computer Engineering, as a project for the NUS CS4222 Wireless Networking module.
