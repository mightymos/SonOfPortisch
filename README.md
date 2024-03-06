# SonOfPortisch
Port of RF-Bridge-EFM8BB1 over to SDCC compiler


Uses:  
https://github.com/Portisch/RF-Bridge-EFM8BB1  
https://github.com/ahtn/efm8_sdcc  

Compiling all features with SDCC exceeds 8KB code space.  
So the state machine and features had to be reduced.  
It would take additional work and thought to restore all functionality.  
Note it is easiest to debug with the EFM8BB1LCK Busy Bee development board combined with a radio receiver.  

Therefore the original Portisch firmware in Simplicity Studio with Keil compiler is attractive.  

An alternative exists for newer Sonoff Bridge R2 v2.2 hardware:  
https://github.com/mightymos/RF-Bridge-OB38S003  
