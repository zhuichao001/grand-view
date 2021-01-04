mkdir build
cmake ..
make

cd ../
mkdir runtime
cp build/lighter_server runtime
cp build/lighter_client runtime

ip='192.168.0.3'
configures='$ip:8100,$ip:8101,$ip:8102'
./lighter_server --port=8100 --conf=$configures
./lighter_server --port=8101 --conf=$configures
./lighter_server --port=8102 --conf=$configures

./lighter_client --conf=$configures
