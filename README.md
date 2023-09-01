# Morse Code Driver BeagleBone
Add this morse code driver on your BeagleBone and have an LED will flash to your text.
Display the dot-dash flash code via Text UI.

## Installation
1. Clone the repository
```bash
(host)$ git clone https://github.com/Tooo/Morse-Code-Driver-BeagleBone.git
```

2. Make excutable file on the host
```bash
(host)$ make
```

3. Load Morse code driver
```bash
(bbg)$ sudo insmod morse-code.ko
```

4. Install morsecode on your LED0
```bash
(bbg)$ echo morse-code > /sys/class/leds/beaglebone\:green\:usr0/trigger
```

## Usage
Flash out the message Hello world
```bash
(bbg)$ echo 'Hello world' | sudo tee /dev/morse-code
```

Display the dot-dash flash code
```bash
(bbg)$ sudo cat /dev/morse-code
```

## Uninstall Driver
```bash
(bbg)$ sudo rmmod morse-code
```


