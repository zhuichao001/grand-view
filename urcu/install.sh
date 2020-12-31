yum install netcdf
yum install libtool

./bootstrap # skip if using tarball
./configure
make
make install
ldconfig
