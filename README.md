![alt text](/cuwo.png)  
( http://cuwo.org/#about )

# MODDED CUWO

A CUWO server (Created by matpow2 & team) modded to fit into the new patch, with a bunch of new minecraft-like commands & custom server ranks.

## Installation

Get into ubuntu18 and install git. Then, create a directory and clone the files into it:

```bash
sudo apt-get install git
mkdir cuwo && cd cuwo
git clone https://github.com/yxticode/cuwo-modded.git
```

When the files are done, simply execute the "autoinstall.sh" script to get everything ready:
```bash
sh autoinstall.sh
```
Wait for the script to finish & all is ready.
## Usage

To initialize the CUWO server, run the "run_server.sh" script. It will load all the modules and get the server up and running in the port 12345!

```bash
sh run_server.sh
```

## Contributing
Pull requests are welcome. For major changes, please open an issue first to discuss what you would like to change.

Please make sure to update tests as appropriate.

## License
Licensed under the [GPL3.0](https://choosealicense.com/licenses/gpl-3.0)
