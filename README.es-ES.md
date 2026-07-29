# CH32V305-UAC2
un dac usb-hs uac2

> [!NOTE]
> el ch32v305fbp6 no tiene OTG-HS (pero tiene OTG-FS), por lo que no se puede conectar este proyecto a un smartphone.

## hardware v1
ch32v305fbp6 + es9018k2m + opa1678

## hardware v2
ch32v305fbp6 + es9018k2m + opa1678 + sgm8262
> [!NOTE]
> es posible que desee cambiar todas las huellas de capacidad de 10uf a 0805, ya que los de 0603 10uf pueden tener solo versiones de 10v
> y es posible que desee agregar más capacidad para la bomba de carga (charge pump)

## aviso

Debido al uso de componentes económicos, el reloj tiene un error de aproximadamente 200 Hz. Además, la velocidad del GPIO MCLK excede el límite máximo de 50 MHz a 192 kHz.
Si lo desea, puede utilizar un cristal externo para el reloj I2S MCLK.
Este dac solo soporta 48k, 96k, 192k 32bits; no se han considerado dsd ni otros formatos.
Este DAC solo soporta conexiones de alta velocidad.
Es posible que escuche algunos chasquidos (popping sounds) durante tareas repentinas de alta carga.

## configuración
Simplemente flashee HSDAC.elf al hardware. No utilice HID-Bootloader ni dfu, se revisarán en el futuro.

## se necesita ayuda
¿Alguien podría rediseñar el circuito del amplificador de potencia?

# IMAGEN
![DAC Image](resource/dacv2.jpg)
