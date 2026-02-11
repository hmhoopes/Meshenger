## Working w/ ESP's
- Follow this guide to install arduino IDE and configure it to work with ESP 32: https://www.espboards.dev/blog/setting-up-arduino-ide-esp32/ 
- I had to follow https://forum.arduino.cc/t/ide-2-3-7-now-gives-error-4-deadline-exceeded/1422321/2 to increase timeout delay to download the esp packages
- I also had to download new USB CP210x drivers to get it to work
- Select Board as "ESP32 Dev Module"
- Use Arduino IDE to upload blinking.ino, which should cause a blue led to flash