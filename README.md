# SonOfPortisch
Port of RF-Bridge-EFM8BB1 over to SDCC compiler


Uses:  
https://github.com/Portisch/RF-Bridge-EFM8BB1  
https://github.com/ahtn/efm8_sdcc  

Compiles but with sniffing enabled takes up 9KB which exceeds code space.  
With sniffing disabled however, basic decoding causes bridge hardware to reset.  

An alternative exists for newer Sonoff Bridge R2 v2.2 hardware:  
https://github.com/mightymos/RF-Bridge-OB38S003  
