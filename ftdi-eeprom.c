/* ftdi-eeprom.c
 * Copyright 2016 Evan Nemerson <evan@nemerson.com>
 * Licensed under the GNU GPLv3; see COPYING for details.
 **/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <libusb.h>
#include <ftdi.h>

static void
print_help (int argc, char* argv[]) {
  (void) argc;

  fprintf (stdout, "USAGE: %s [OPTIONS] bus:device\n", argv[0]);
  fprintf (stdout, "Use lsusb to find the bus and device numbers.\n");
  fprintf (stdout, "Options:\n");
  fprintf (stdout, "  -m, --manufacturer=MFG Set the manufacturer to the specified value\n");
  fprintf (stdout, "  -p, --product=PRODUCT  Set the product to the specified value\n");
  fprintf (stdout, "  -s, --serial=SERIAL    Set the serial to the specified value\n");
  fprintf (stdout, "  -v, --verbose          Verbose output\n");
  fprintf (stdout, "  -h, --help             Print this help screen and exit\n");
  fprintf (stdout, "Report bugs to <https://github.com/nemequ/ftdi-eeprom>\n");
}

static bool verbose_mode = false;

static void
print_verbose (const char* fmt, ...) {
  if (verbose_mode) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
  }
}

static bool
parse_device_identifier (int* bus, int* device, const char* identifier) {
  char* endptr = NULL;
  long tmp = strtol(identifier, &endptr, 0);
  if (*endptr != ':')
    return false;
  *bus = (int) tmp;
  tmp = strtol(endptr + 1, &endptr, 0);
  if (*endptr != '\0')
    return false;
  *device = (int) tmp;
  return true;
}

int
main(int argc, char *argv[]) {
  int c;
  static struct option long_options[] = {
    {"manufacturer", 1, 0, 'm'},
    {"product",      1, 0, 'p'},
    {"serial",       1, 0, 's'},
    {"verbose",      0, 0, 'v'},
    {"help",         0, 0, 'h'},
    {NULL, 0, NULL, 0}
  };
  char orig_manufacturer[128];
  char* manufacturer = NULL;
  char orig_product[128];
  char* product = NULL;
  char orig_serial[128];
  char* serial = NULL;
  int bus = -1;
  int device = -1;
  struct ftdi_context ftdi;
  int ires = 0;

  while ((c = getopt_long(argc, argv, "m:p:s:vh", long_options, &optind)) != -1) {
    switch (c) {
      case 'm':
	manufacturer = optarg;
	break;
      case 'p':
	product = optarg;
	break;
      case 's':
	serial = optarg;
	break;
      case 'v':
	verbose_mode = true;
	break;
      case 'h':
	print_help(argc, argv);
	return EXIT_SUCCESS;
	break;
    }
  }

  if (optind + 1 != argc) {
    print_help(argc, argv);
    return EXIT_FAILURE;
  } else {
    if (!parse_device_identifier(&bus, &device, argv[optind])) {
      fprintf(stderr, "Unable to parse `%s'\n", argv[optind]);
      return EXIT_FAILURE;
    }
    print_verbose("Using device %03d:%03d\n", bus, device);
  }

  libusb_init(NULL);
  libusb_device** usb_devices;
  ssize_t n_devices = libusb_get_device_list(NULL, &usb_devices);
  if (n_devices < 0) {
    fprintf(stderr, "ERROR: Unable to list USB devices [%zd]: %s\n", n_devices, libusb_strerror((enum libusb_error) n_devices));
    return EXIT_FAILURE;
  }

  libusb_device* usb_device = NULL;
  for (int i = 0 ; i < n_devices ; i++) {
    if (libusb_get_bus_number(usb_devices[i]) == bus &&
	libusb_get_device_address(usb_devices[i]) == device) {
      usb_device = usb_devices[i];
      break;
    }
  }
  if (usb_device == NULL) {
    fprintf(stderr, "ERROR: Unable to find USB device at %03d:%03d\n", bus, device);
    return EXIT_FAILURE;
  }

  if ((ires = ftdi_init(&ftdi)) != 0) {
    fprintf(stderr, "ERROR: Unable to initialize libftdi (%d)\n", ires);
    return EXIT_FAILURE;
  }

  if ((ires = ftdi_usb_open_dev(&ftdi, usb_device)) != 0) {
    fprintf(stderr, "ERROR: Unable to open device (%d)\n", ires);
    fprintf(stderr, "       Perhaps you don't have sufficient permissions (i.e., you aren't root)?\n");
    return EXIT_FAILURE;
  }

  {
    struct libusb_device_descriptor desc;

    if ((ires = libusb_get_device_descriptor(libusb_get_device (ftdi.usb_dev), &desc)) < 0) {
      fprintf(stderr, "ERROR: Unable to retrieve device descriptor (%d): %s\n", ires, libusb_strerror((enum libusb_error) ires));
      return EXIT_FAILURE;
    }

    if ((ires = libusb_get_string_descriptor_ascii(ftdi.usb_dev, desc.iManufacturer, (unsigned char*) orig_manufacturer, sizeof(orig_manufacturer))) < 0) {
      fprintf(stderr, "ERROR: Unable to retrieve manufacturer (%d): %s\n", ires, libusb_strerror((enum libusb_error) ires));
      return EXIT_FAILURE;
    }
    print_verbose("Old manufacturer: %s\n", orig_manufacturer);
    if (manufacturer == NULL)
      manufacturer = orig_manufacturer;

    if ((ires = libusb_get_string_descriptor_ascii(ftdi.usb_dev, desc.iProduct, (unsigned char*) orig_product, sizeof(orig_product))) < 0) {
      fprintf(stderr, "ERROR: Unable to retrieve product (%d): %s\n", ires, libusb_strerror((enum libusb_error) ires));
      return EXIT_FAILURE;
    }
    print_verbose("Old product:      %s\n", orig_product);
    if (product == NULL)
      product = orig_product;

    if ((ires = libusb_get_string_descriptor_ascii(ftdi.usb_dev, desc.iSerialNumber, (unsigned char*) orig_serial, sizeof(orig_serial))) < 0) {
      fprintf(stderr, "ERROR: Unable to retrieve serial (%d): %s\n", ires, libusb_strerror((enum libusb_error) ires));
      return EXIT_FAILURE;
    }
    print_verbose("Old serial:       %s\n", orig_serial);
    if (serial == NULL)
      serial = orig_serial;
  }

  print_verbose("New manufacturer: %s\n", manufacturer);
  print_verbose("New product:      %s\n", product);
  print_verbose("New serial:       %s\n", serial);

  if ((ires = ftdi_eeprom_initdefaults (&ftdi, manufacturer, product, serial)) < 0) {
    fprintf(stderr, "Unable to set EEPROM defaults: %d\n", ires);
    return EXIT_FAILURE;
  }

  if ((ires = ftdi_eeprom_build (&ftdi)) < 0) {
    fprintf(stderr, "Unable to build EEPROM: %d\n", ires);
    return EXIT_FAILURE;
  }

  if ((ires = ftdi_write_eeprom (&ftdi)) < 0) {
    fprintf(stderr, "Unable to write EEPROM: %d\n", ires);
    return EXIT_FAILURE;
  }

  ftdi_deinit(&ftdi);

  return EXIT_SUCCESS;
}
