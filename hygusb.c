/*
 * /usr/share/doc/collectd/examples/hygusb.c
 *
 * A plugin template for collectd.
 *
 * Written by Sebastian Harl <sh@tokkee.org>
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; only version 2 of the License is applicable.
 */

/*
 * Notes:
 * - plugins are executed in parallel, thus, thread-safe
 *   functions need to be used
 * - each of the functions below (except module_register)
 *   is optional
 */

#if ! HAVE_CONFIG_H

#include <stdlib.h>

#include <string.h>

#ifndef __USE_ISOC99 /* required for NAN */
# define DISABLE_ISOC99 1
# define __USE_ISOC99 1
#endif /* !defined(__USE_ISOC99) */
#include <math.h>
#if DISABLE_ISOC99
# undef DISABLE_ISOC99
# undef __USE_ISOC99
#endif /* DISABLE_ISOC99 */

#include <time.h>
#include <stdint.h>

#endif /* ! HAVE_CONFIG */

#include <collectd/core/collectd.h>
#include <collectd/core/common.h>
#include <collectd/core/plugin.h>

#include <libusb.h>

#define FALSE 0
#define TRUE !FALSE

#define HYGUSB_VID      0x04D8
#define HYGUSB_PID      0xF2C4

int selectBus = 0, selectAddress = 0 ;
float humidityWarning = 0, humidityAlert = 0;
float tempWarning = 0, tempAlert = 0;

typedef union {
    struct {
        u_int8_t msb ;
        u_int8_t lsb ;
    } ;
    u_int16_t value ;
} __DWORD ;

typedef struct __attribute__ ( ( packed ) ) {
    __DWORD hyg ;
    __DWORD temp ;

    u_int8_t green_led ;
    u_int8_t yellow_led ;
    u_int8_t red_led ;
    signed char parity ;

} __INTERNAL_DEVSTATE ;

int parity_check ( __INTERNAL_DEVSTATE __dev_state ) {
    unsigned char *data ;

    if ( __dev_state.parity == 0 ) {
        /* Original firmware ? */
        /* can only confirm known data are ok */

        if ( __dev_state.green_led != 0
             && __dev_state.green_led != 1
             && __dev_state.green_led != 0xFF )
            return FALSE ;

        if ( __dev_state.yellow_led != 0
             && __dev_state.yellow_led != 1
             && __dev_state.yellow_led != 0xFF )
            return FALSE ;

        if ( __dev_state.red_led != 0
             && __dev_state.red_led != 1
             && __dev_state.red_led != 0xFF )
            return FALSE ;

        return TRUE ;
    } else {
        data = ( unsigned char * ) &__dev_state ;
        return data[7] ==
            ( data[0] ^ data[1] ^ data[2] ^ data[3] ^ data[4] ^
              data[5] ^ data[6] ) ;
    }
}

/* submit data to collectd  */
static void hygusb_submit (const char* plugin_instance, const char* type, gauge_t value) {
    value_t values[1] ;
    value_list_t vl = VALUE_LIST_INIT ;

    values[0].gauge = value ;

    vl.values     = values ;
    vl.values_len = STATIC_ARRAY_SIZE (values) ;

    sstrncpy ( vl.host, hostname_g, sizeof (vl.host) ) ;
    sstrncpy ( vl.plugin, "hygusb", sizeof (vl.plugin) ) ;
    sstrncpy ( vl.plugin_instance, plugin_instance,
               sizeof (vl.plugin_instance) ) ;

    sstrncpy ( vl.type, type, sizeof (vl.type) );

    plugin_dispatch_values (&vl) ;
}

static int hygusb_process_device ( libusb_device_handle * handle,
                            const char *plugin_instance ) {

    int i, r ;
    int transferred ;

    __INTERNAL_DEVSTATE __dev_state ;
    unsigned char data_out[4] = { 'D', 'D', 'D', 'A' } ;

    int hyg_data ;
    int temp_data ;

    float hyg ;
    float temp ;


    // Read Data
    int count = 0 ;
    int success = 0 ;

    do {
        count++ ;

        // Send Data (initialize IN transfert for some firmware Rev)
        r = libusb_interrupt_transfer ( handle, 0x01, data_out, 4,
                                        &transferred, 5000 ) ;
        if ( r != 0 ) {
            ERROR ( "Could not send data to hyg-usb (%s). Exiting.\n",
                    libusb_error_name ( r ) ) ;
            continue ;
        }
        if ( transferred < 4 ) {
            ERROR ( "Short write to hyg-usb. Exiting.\n" ) ;
            continue ;
        }

        r = libusb_interrupt_transfer ( handle, 0x81,
                                        ( unsigned char * )
                                        &__dev_state,
                                        sizeof ( __INTERNAL_DEVSTATE ),
                                        &transferred, 5000 ) ;

        if ( r != 0 ) {
            ERROR ( "Could not read data from hyg-usb (%s). Retrying ...\n",
                    libusb_error_name ( r ) ) ;
            continue ;
        }

        if ( transferred < 8 ) {
            ERROR ( "Short read from hyg-usb. Retrying ...\n" ) ;
            continue ;
        }

        if ( !parity_check ( __dev_state ) ) {
            ERROR ( "Parity Check Failed. Retrying ...\n" ) ;
            continue ;
        }

        success = 1 ;
    } while ( !success && ( count < 3 ) ) ;


    if ( !success ) {

        ERROR ("Still failing after %d attempts. Exiting.\n",
               count ) ;
        return EXIT_FAILURE ;
    }

    // *** Display Hyg
    hyg = 125.0 * ntohs ( __dev_state.hyg.value ) / 65536.0 - 6.0 ;

    // *** Display Temp
    temp = 175.72 * ntohs ( __dev_state.temp.value ) / 65536.0 - 46.85 ;

    hygusb_submit ( plugin_instance, "humidity", hyg ) ;
    hygusb_submit ( plugin_instance, "temperature", temp ) ;

    return EXIT_SUCCESS ;
}


/*
 * Collectd Entry points
 */

/*
 * This function is called once upon startup to initialize the plugin.
 */
static int hygusb_init (void)
{
    /* open sockets, initialize data structures, ... */
    int r = libusb_init ( NULL );
    if ( r != 0 ) {
        ERROR ("Could not initialize libusb (%s)", libusb_error_name ( r ) ) ;
        return r ;
    }

    /* A return value != 0 indicates an error and causes the plugin to be
       disabled. */
    return 0;
} /* static int hygusb_init (void) */

/*
 * This function is called in regular intervalls to collect the data.
 */
static int hygusb_read (void)
{
    libusb_device **devs ;
    libusb_device *dev ;
    libusb_device_handle *handle = NULL ;

    char red_led = 'D' ;
    char green_led = 'D' ;
    char yellow_led = 'D' ;

    char plugin_instance[DATA_MAX_NAME_LEN] ;

    int i, r ;
    ssize_t cnt ;

    cnt = libusb_get_device_list ( NULL, &devs ) ;
    if ( cnt < 0 ) {
        ERROR ( "Could not find any usb devices. Exiting\n" ) ;
        return ( int )cnt ;
    }

    i = 0 ;
    while ( ( dev = devs[i++] ) != NULL ) {
        struct libusb_device_descriptor desc ;

        r = libusb_get_device_descriptor ( dev, &desc ) ;
        if ( r < 0 ) {
            ERROR ( "failed to get device descriptor (%s). Exiting\n",
                    libusb_error_name ( r ) ) ;
            return r ;
        }

        if ( desc.idVendor == HYGUSB_VID
             && desc.idProduct == HYGUSB_PID ) {

            int devBus = libusb_get_bus_number ( dev ) ;
            int devAddress = libusb_get_device_address ( dev ) ;

            DEBUG ( "Found HYGUSB (%04x:%04x) at [%03d:%03d]\n",
                    desc.idVendor, desc.idProduct, devBus,
                    devAddress ) ;

            if ( ( selectBus > 0 && devBus != selectBus ) ||
                 ( selectAddress > 0
                   && devAddress != selectAddress ) )
                continue ;

            r = libusb_open ( dev, &handle ) ;
            if ( r != 0 ) {
                ERROR ( "Could not open device (%s), skipping\n",
                        libusb_error_name ( r ) ) ;
                continue ;
            }

            r = libusb_claim_interface ( handle, 0 ) ;
            if ( r != 0 ) {
                ERROR ( "Could not claim usb interface (%s). skipping\n",
                        libusb_error_name ( r ) ) ;
                continue ;
            }


            if (desc.iSerialNumber > 0) {
                r = libusb_get_string_descriptor_ascii ( handle, desc.iSerialNumber, plugin_instance, DATA_MAX_NAME_LEN );

                if (r < 0) {
                    snprintf (plugin_instance, DATA_MAX_NAME_LEN,
                            "%03d.%03d.0", devBus, devAddress ) ;
                } else {
                    strncat (plugin_instance, ".0", DATA_MAX_NAME_LEN - r -1);
                }
            } else {
                snprintf (plugin_instance, DATA_MAX_NAME_LEN,
                        "%03d.%03d.0", devBus, devAddress ) ;
            }

            r = hygusb_process_device ( handle, plugin_instance ) ;

            libusb_close ( handle ) ;

            fprintf ( stdout, "\n" ) ;
        }
    }

    libusb_free_device_list ( devs, 1 ) ;
    return 0;
}

/*
 * This function is called before shutting down collectd.
 */
static int hygusb_shutdown (void) {
    /* close sockets, free data structures, ... */

    libusb_exit ( NULL ) ;
    return 0;
} /* static int hygusb_shutdown (void) */

/*
 * This function is called after loading the plugin to register it with
 * collectd.
 */
void module_register (void) {
    plugin_register_read ("hygusb", hygusb_read);
    plugin_register_init ("hygusb", hygusb_init);
    plugin_register_shutdown ("hygusb", hygusb_shutdown);
    return;
} /* void module_register (void) */
