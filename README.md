# CH32V305-UAC2
ch32v305fbp6 + es9018k2m + opa1678 usb dac  

> [!WARNING]
> This branch may not be maintenance in future.  

> [!NOTE]
> Due to using inexpensive components, the clock has approximately a 200 Hz error. Additionally, the MCLK GPIO speed exceeds the maximum 50 MHz limit at 192 kHz.
> If desired, you can use an external crystal for the I2S MCLK clock.

> [!NOTE]
> this dac only support 48k,96k,192k 32bits, dsd and other formats are not considered

> [!NOTE]
> This DAC only supports high-speed connections, so do not connect it through a USB hub.
> Any clicks that occur are a result of the computer not sending data within a given time. Therefore, you may hear clicks during periods of high system load.
# IMAGE
![DAC Image](resource/dac.jpg)

