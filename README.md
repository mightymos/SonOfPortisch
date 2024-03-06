# SonOfPortisch
Port of RF-Bridge-EFM8BB1 over to SDCC compiler


Uses:  
https://github.com/Portisch/RF-Bridge-EFM8BB1  
https://github.com/ahtn/efm8_sdcc  

Compiling all features with SDCC exceeds 8KB code space.
Therefore the original Portisch firmware in Simplicity Studio with Keil compiler is attractive.
It is easier to debug with the EFM8BB1LCK Busy Bee development board.

An alternative exists for newer Sonoff Bridge R2 v2.2 hardware:  
https://github.com/mightymos/RF-Bridge-OB38S003  
