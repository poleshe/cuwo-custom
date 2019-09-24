
# INSTALL PYTHON 3.6 & INSTALL CUWO DEPENDENCIES (Cython, also numpy and pyrr sometimes dont get installed so we add them too)

echo "------------------------------"
echo "-- Installing dependencies  --"
echo "------------------------------"

sudo apt-get install -y python3.6 python3.6-dev python3-pip build-essential

# Install cython

sudo pip3 install cython

# Install numpy & pyrr

sudo pip3 install numpy

sudo pip3 install pyrr

# Execute the python build-up

echo "------------------------------"
echo "--  Building python module  --"
echo "------------------------------"

python3.6  -W ignore setup.py build_ext --inplace

# Give chmod permissions to the script just in case

echo "------------------------------------"
echo "--  Making run_server executable  --"
echo "------------------------------------"

sudo chmod +x run_server.sh

echo "----------------------------------------------------------------------------"
echo "-- Remember to check your config at base.py and change the login password --"
echo "--                   To start the cuwo server, use run_server.sh          --"
echo "--                            Have a good time!!                          --"
echo "----------------------------------------------------------------------------"
