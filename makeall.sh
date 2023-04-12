# Change to script folder and execute main entry point
cd code/r_start/
touch *
make clean
make
cd ../..

cd code/r_utils/
touch *
make clean
make
cd ../..

cd code/r_vehicle/
touch *
make clean
make
cd ../..

cd code/r_i2c/
touch *
make clean
make
cd ../..

cd code/r_plugins_osd/
touch *
make clean
make
cd ../..

cd code/r_station/
touch *
make clean
make
cd ../..

cd code/r_central/
touch *
make clean
make
cd ../..


