# hyg-usb_collectd

hyg-usb is a small temperature and relative humidity sensor YOU CAN HACK. It is based on a PIC 16F1455
and a Si7021 sensor and comes with a preinstalled firmware ( see http://www.macareux-labs.com for details )

This is a collectd plugin to allow simple integration on your monitoring infrastructure.

## Installation

### Dependencies

To compile this project, you will need to install:

	1. libusb-1.0
        2. collectd-dev

If you are running Debian or one of its siblings, just type as root :

	apt-get install libusb-1.0-0-dev collectd-dev

### Getting the sources

The sources are available on github: https://github.com/bplessis/hyg-usb_collectd
You can retrieve them as a zip file with your favorite web browser or,better, just use git.

### Compiling and installing

        $ make

        $ [sudo] cp hygusb.so /usr/lib/collectd/


## Synopsis

        Add "LoadPlugin hygusb" in your collectd

## Options


## Contributing

1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Commit your changes: `git commit -am 'Add some feature'`
4. Push to the branch: `git push origin my-new-feature`
5. Submit a pull request !

## History

Mar 2016: V0.1 - First release

## License

This project is placed under the MIT License. See LICENSE file for details
