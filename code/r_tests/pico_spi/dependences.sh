raspi-config --expand-rootfs
sudo apt install minicom

git config --global http.sslVerify false

git clone -b master https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init
cd ..
git clone -b master https://github.com/raspberrypi/pico-examples.git

sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

export PICO_SDK_PATH=/home/pi/pico/pico-sdk
cd build
cmake ../

minicom -b 115200 -o -D /dev/ttyACM0

