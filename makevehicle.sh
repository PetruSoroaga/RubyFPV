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
