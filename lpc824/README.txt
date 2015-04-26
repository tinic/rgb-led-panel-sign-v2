
This is a minimal setup for writing projects for the NXP LPC82x (LPC824 & LPC822) 
series of MCUs on Debian based Linux distributions (Debian, Ubuntu, LinuxMINT etc.)

1. run 'bootstrap.sh', it will install the proper compiler and compile lpc21isp
2. run 'make' to compile the project and upload the firmware to the chip

For the upload to work you need a USB to serial converter with a working RTS
and CTS line. Most of the available USB2serial dongles do not offer an RTS
breakout pin but can be easily modified by manually connecting the RTS pin 
from the FTDI (or similar) chip itself to the LPC824 ~RESET pin. Alternatively
you can solder a wire from the RTS pin of the FTDI (or similar) chip to the CTS
breakout pin of the USB2serial dongle as the CTS pin is not used.

Connect the lines as following:

Dongle             LPC82x
 RTS      -->  ~RESET  (PIO0_5)
 DTR      -->   ISP    (PIO0_12)
 TX       -->   U0_RXD (PIO0_0)
 RX       -->   U0_TXD (PIO0_4)
 GND      -->   VSS

