/*******************************************************************************\
 * Program Name       :   libcyusb.cpp                                         *
 * Author             :   V. Radhakrishnan ( rk@atr-labs.com )                 *
 * License            :   LGPL Ver 2.1                                         *
 * Copyright          :   Cypress Semiconductors Inc. / ATR-LABS               *
 * Date written       :   March 12, 2012                                       *
 * Modification Notes :                                                        *
 *                                                                             *
 * This program is the main library for all cyusb applications to use.         *
 * This is a thin wrapper around libusb                                        *
 \******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cyusb.h>

#ifdef __ANDROID__
#define  LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE,"LIB-USB-libcyusb",__VA_ARGS__)
static int fd;
#endif
/* Maximum length of a string read from the Configuration file (/etc/cyusb.conf) for the library. */
#define MAX_CFG_LINE_LENGTH                     (120)

/* Maximum length for a filename. */
#define MAX_FILEPATH_LENGTH         (256)

/* Maximum size of EZ-USB FX3 firmware binary. Limited by amount of RAM available. */
#define FX3_MAX_FW_SIZE             (524288)

/* Cypress fx3s firmware dowmload info */
#define MAX_FWIMG_SIZE      (512 * 1024)    // Maximum size of the firmware binary.
#define MAX_WRITE_SIZE      (2 * 1024)  // Max. size of data that can be written through one vendor command.

#define VENDORCMD_TIMEOUT   (5000)      // T/DatatypeConverterimeout (in milliseconds) for each vendor command.
#define GETHANDLE_TIMEOUT   (5)     // Timeout (in seconds) for getting a FX3 flash programmer handle.

/* Utility macros. */
#define ROUND_UP(n,v)   ((((n) + ((v) - 1)) / (v)) * (v))   // Round n upto a multiple of v.
#define GET_LSW(v)  ((unsigned short)((v) & 0xFFFF))    // Get Least Significant Word part of an integer.
#define GET_MSW(v)  ((unsigned short)((v) >> 16))       // Get Most Significant Word part of an integer.

/* Array representing physical size of EEPROM corresponding to each size encoding. */
const int i2c_eeprom_size[] =
        {
                1024,       // bImageCtl[2:0] = 'b000
                2048,       // bImageCtl[2:0] = 'b001
                4096,       // bImageCtl[2:0] = 'b010
                8192,       // bImageCtl[2:0] = 'b011
                16384,      // bImageCtl[2:0] = 'b100
                32768,      // bImageCtl[2:0] = 'b101
                65536,      // bImageCtl[2:0] = 'b110
                131072      // bImageCtl[2:0] = 'b111
        };
/* end of Cypress fx3s firmware dowmload info */

static struct cydev     cydev[MAXDEVICES];      /* List of devices of interest that are connected. */
static int              nid;                /* Number of Interesting Devices. */
static libusb_device    **list;             /* libusb device list used by the cyusb library. */

/*
   struct VPD
   Used to store information about the devices of interest listed in /etc/cyusb.conf
 */
struct VPD {
    unsigned short  vid;                /* USB Vendor ID. */
    unsigned short  pid;                /* USB Product ID. */
    char        desc[MAX_STR_LEN];      /* Device description. */
};

static struct VPD   vpd[MAX_ID_PAIRS];      /* Known device database. */
static int      maxdevices;         /* Number of devices in the vpd database. */
static unsigned int     checksum = 0;           /* Checksum calculated on the Cypress firmware binary. */

/* The following variables are used by the cyusb_linux application. */
char        pidfile[MAX_FILEPATH_LENGTH];   /* Full path to the PID file specified in /etc/cyusb.conf */
char        logfile[MAX_FILEPATH_LENGTH];   /* Full path to the LOG file specified in /etc/cyusb.conf */
int     logfd;              /* File descriptor for the LOG file. */
int     pidfd;              /* File descriptor for the PID file. */

#ifdef __ANDROID__
static libusb_device_handle* get_cyusb_handle();
static int usb_fd = 0;
#endif

/* isempty:
   Check if the first L characters of the string buf are white-space characters.
 */
static bool
isempty (
        char *buf,
        int L)
{
    bool flag = true;
    int i;

    for (i = 0; i < L; ++i ) {
        if ( (buf[i] != ' ') && ( buf[i] != '\t' ) ) {
            flag = false;
            break;
        }
    }

    return flag;
}

/* parse_configfile:
   Parse the /etc/cyusb.conf file and get the list of USB devices of interest.
 */
static void
parse_configfile (
        void)
{
    FILE *inp;
    char buf[MAX_CFG_LINE_LENGTH];
    char *cp1, *cp2, *cp3;
    //  int i;

    inp = fopen("/etc/cyusb.conf", "r");
    if (inp == NULL)
        return;

    memset(buf,'\0',MAX_CFG_LINE_LENGTH);
    while ( fgets(buf,MAX_CFG_LINE_LENGTH,inp) ) {
        if ( buf[0] == '#' )            /* Any line starting with a # is a comment  */
            continue;
        if ( buf[0] == '\n' )
            continue;
        if ( isempty(buf,strlen(buf)) )     /* Any blank line is also ignored       */
            continue;

        cp1 = strtok(buf," =\t\n");
        if ( !strcmp(cp1,"LogFile") ) {
            cp2 = strtok(NULL," \t\n");
            strcpy(logfile,cp2);
        }
        else if ( !strcmp(cp1,"PIDFile") ) {
            cp2 = strtok(NULL," \t\n");
            strcpy(pidfile,cp2);
        }
        else if ( !strcmp(cp1,"<VPD>") ) {
            while ( fgets(buf,MAX_CFG_LINE_LENGTH,inp) ) {
                if ( buf[0] == '#' )        /* Any line starting with a # is a comment  */
                    continue;
                if ( buf[0] == '\n' )
                    continue;
                if ( isempty(buf,strlen(buf)) ) /* Any blank line is also ignored       */
                    continue;
                if ( maxdevices == (MAX_ID_PAIRS - 1) )
                    continue;
                cp1 = strtok(buf," \t\n");
                if ( !strcmp(cp1,"</VPD>") )
                    break;
                cp2 = strtok(NULL, " \t");
                cp3 = strtok(NULL, " \t\n");

                vpd[maxdevices].vid = strtol(cp1,NULL,16);
                vpd[maxdevices].pid = strtol(cp2,NULL,16);
                strncpy(vpd[maxdevices].desc,cp3,MAX_STR_LEN);
                vpd[maxdevices].desc[MAX_STR_LEN - 1] = '\0';   /* Make sure of NULL-termination. */

                ++maxdevices;
            }
        }
        else {
            printf("Error in config file /etc/cyusb.conf: %s \n",buf);
            exit(1);
        }
    }

    fclose(inp);
}

/* device_is_of_interest:
   Check whether the current USB device is among the devices of interest.
 */
static int
device_is_of_interest (
        cyusb_device *d)
{
    int i;
    int found = 0;
    struct libusb_device_descriptor desc;
    //int vid;
    //int pid;

    libusb_get_device_descriptor(d, &desc);
    //vid = desc.idProduct;
    //pid = desc.idProduct;

    for ( i = 0; i < maxdevices; ++i ) {
        if ( (vpd[i].vid == desc.idVendor) && (vpd[i].pid == desc.idProduct) ) {
            found = 1;
            break;
        }
    }
    return found;
}

/* cyusb_getvendor:
   Get the Vendor ID for the current USB device.
 */
unsigned short
cyusb_getvendor (
        cyusb_handle *h)
{
    struct libusb_device_descriptor d;
    cyusb_get_device_descriptor(h, &d);
    return d.idVendor;
}

/* cyusb_getproduct:
   Get the Product ID for the current USB device.
 */
unsigned short
cyusb_getproduct (
        cyusb_handle *h)
{
    struct libusb_device_descriptor d;
    cyusb_get_device_descriptor(h, &d);
    return d.idProduct;
}

/* renumerate:
   Get handles to and store information about all USB devices of interest.
 */
static int
renumerate (
        void)
{
    //  cyusb_device *dev = NULL;
    cyusb_handle *handle = NULL;
    int           numdev;
    //  int           found = 0;
    int           i;
    int           r;

    numdev = libusb_get_device_list(NULL, &list);
    if ( numdev < 0 ) {
#ifdef __ANDROID__
        LOGV("Library: Error in enumerating devices...");
#else
        printf("Library: Error in enumerating devices...\n");
#endif
        return -ENODEV;
    }

    nid = 0;
    for ( i = 0; i < numdev; ++i ) {
        cyusb_device *tdev = list[i];
        if ( device_is_of_interest(tdev) ) {
            cydev[nid].dev = tdev;
            r = libusb_open(tdev, &cydev[nid].handle);
            if ( r ) {
                printf("Error in opening device\n");
                return -EACCES;
            }
            else
                handle = cydev[nid].handle;

            cydev[nid].vid     = cyusb_getvendor(handle);
            cydev[nid].pid     = cyusb_getproduct(handle);
            cydev[nid].is_open = 1;
            cydev[nid].busnum  = cyusb_get_busnumber(handle);
            cydev[nid].devaddr = cyusb_get_devaddr(handle);
            ++nid;
        }
    }

    return nid;
}

static int fx3_ram_write(cyusb_handle  *h, unsigned char *buf, unsigned int ramAddress, int len)
{
    int r;
    int index = 0;
    int size;

    while (len > 0)
    {
        size = (len > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : len;
        r = cyusb_control_transfer(h, 0x40, 0xA0, GET_LSW(ramAddress), GET_MSW(ramAddress),
                                   &buf[index], size, VENDORCMD_TIMEOUT);
        if (r != size)
        {
#ifdef __ANDROID__
            LOGV("Vendor write to FX3 RAM failed");
#else
            printf("Vendor write to FX3 RAM failed");
#endif
            return -1;
        }

        ramAddress += size;
        index += size;
        len -= size;
    }

    return 0;
}

static int read_firmware_image(const char *filename, unsigned char *buf, int *romsize, int *filesize)
{
    int fd;
    int nbr = 0;
    struct stat filestat;

    if (stat(filename, &filestat) != 0)
    {
#ifdef __ANDROID__
        LOGV("Failed to stat file ");
#else
        printf("Failed to stat file ");
#endif
        return -1;
    }

    // Verify that the file size does not exceed our limits.
    *filesize = filestat.st_size;
    if (*filesize > MAX_FWIMG_SIZE)
    {
#ifdef __ANDROID__
        LOGV("File size exceeds maximum firmware imageDownloadFx3sFirmware size");
#else
        printf("File size exceeds maximum firmware imageDownloadFx3sFirmware size");
#endif
        return -2;
    }

    fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
#ifdef __ANDROID__
        LOGV("File not found");
#else
        printf("File not found");
#endif
        return -3;
    }
    nbr = read(fd, buf, 2);     /* Read first 2 bytes, must be equal to 'CY'    */
    if (nbr == 0)
    {
        if (strncmp((char *)buf, "CY", 2))
        {
#ifdef __ANDROID__
            LOGV("Image does not have 'CY' at start.");
#else
            printf("Image does not have 'CY' at start.");
#endif
            return -4;
        }
    }
    nbr = read(fd, buf, 1);     /* Read 1 byte. bImageCTL   */
    if (nbr == 0)
    {
        if (buf[0] & 0x01)
        {
#ifdef __ANDROID__
            LOGV("Image does not contain executable code");
#else
            printf("Image does not contain executable code");
#endif
            return -5;
        }
    }
    if (romsize != 0)
        *romsize = i2c_eeprom_size[(buf[0] >> 1) & 0x07];

    nbr = read(fd, buf, 1);     /* Read 1 byte. bImageType  */
    if (!(buf[0] == 0xB0))
    {
#ifdef __ANDROID__
        LOGV("Not a normal FW binary with checksum");
#else
        printf("Not a normal FW binary with checksum");
#endif
        return -6;
    }

    // Read the complete firmware binary into a local buffer.
    lseek(fd, 0, SEEK_SET);
    nbr = read(fd, buf, *filesize);

    close(fd);
    return 0;
}


/* cyusb_open:
   Opens handles to all USB devices of interest, and returns their count.
 */
int cyusb_open (
        void)
{
    int fd1;
    int r;

    /*
    <usb-device vendor-id="1204" product-id="240" />    <!-- SL FX3 post-bootloader -->
    <usb-device vendor-id="1204" product-id="243" />    <!-- cypress bootloader product id-->

    * <VPD>
        04b4    8613        Default Cypress USB device
        04b4    4717        Default Cypress USB device
        04b4    0082        FX2 Device: Custom PID from EEPROM
        04b4    1004        FX2LP Bulk Device
        04b4    1003        FX2LP CY-Stream Device
        04b4    00BC        Cypress Benicia device
        04b4    00F0        FX3 Example Application
        04b4    00F1        FX3 Streamer Example
        04b4    00F3        FX3 Boot-loader
        04b4    4720        FX3 I2C/SPI Programmer
        04b4    1005        FX3 2nd stage boot-loader
    </VPD>

    */

#ifdef __ANDROID__
    //jjustman-2020-07-20 - do not attempt to patch virtual devices, libusb based re-numeration will fail...
    r = libusb_init(NULL);
    if(r) {
        LOGV("cyusb_open(void): Error in initializing libusb library, r: %d", r);
        return -EACCES;
    }

    return r;

//    vpd[maxdevices].vid = 1204;
//    vpd[maxdevices].pid = 243;
//    strncpy((char*)&vpd[maxdevices].desc, "FX3 Boot-loader", MAX_STR_LEN);
//    ++maxdevices;

#else
    fd1 = open("/etc/cyusb.conf", O_RDONLY);
    if ( fd1 < 0 ) {
        printf("/etc/cyusb.conf file not found. Exiting\n");
        return -ENOENT;
    }
    else {
        close(fd1);
        parse_configfile(); /* Parse the file and store information inside exported data structures */
    }
    r = libusb_init(NULL);
    if (r) {
        printf("cyusb_open(void): Error in initializing libusb library, r: %d", r);
        return -EACCES;
    }

    /* Get list of USB devices of interest. */
    r = renumerate();
    return r;
#endif
}

/* cyusb_open:
   Open a handle to the USB device with specified vid/pid.
 */
int cyusb_open (
        unsigned short vid,
        unsigned short pid)
{
    int r;
    cyusb_handle *h = NULL;

    r = libusb_init(NULL);
    if (r) {
        printf("Error in initializing libusb library...\n");
        return -EACCES;
    }

    h = libusb_open_device_with_vid_pid(NULL, vid, pid);
    if ( !h ) {
        printf("Device not found\n");
        return -ENODEV;
    }

    cydev[0].dev     = libusb_get_device(h);
    cydev[0].handle  = h;
    cydev[0].vid     = cyusb_getvendor(h);
    cydev[0].pid     = cyusb_getproduct(h);
    cydev[0].is_open = 1;
    cydev[0].busnum  = cyusb_get_busnumber(h);
    cydev[0].devaddr = cyusb_get_devaddr(h);
    nid = 1;

    return 1;
}

/* cyusb_error:
   Print verbose information about the error returned by the cyusb API. These are essentially descriptions of
   status values defined as part of the libusb library.
 */
void
cyusb_error (
        int err)
{
    switch (err)
    {
        case -1:
            fprintf(stderr, "Input/output error\n");
            break;
        case -2:
            fprintf(stderr, "Invalid parameter\n");
            break;
        case -3:
            fprintf(stderr, "Access denied (insufficient permissions)\n");
            break;
        case -4:
            fprintf(stderr, "No such device. Disconnected...?\n");
            break;
        case -5:
            fprintf(stderr, "Entity not found\n");
            break;
        case -6:
            fprintf(stderr, "Resource busy\n");
            break;
        case -7:
            fprintf(stderr, "Operation timed out\n");
            break;
        case -8:
            fprintf(stderr, "Overflow\n");
            break;
        case -9:
            fprintf(stderr, "Pipe error\n");
            break;
        case -10:
            fprintf(stderr, "System call interrupted, ( due to signal ? )\n");
            break;
        case -11:
            fprintf(stderr, "Insufficient memory\n");
            break;
        case -12:
            fprintf(stderr, "Operation not supported/implemented\n");
            break;
        default:
            fprintf(stderr, "Unknown internal error\n");
            break;
    }
}

/* cyusb_gethandle:
   Get a handle to the USB device with specified index.
 */
cyusb_handle *
cyusb_gethandle (
        int index)
{
#ifdef __ANDROID__
    return get_cyusb_handle();
#else
    return cydev[index].handle;
#endif
}

/* cyusb_close:
   Close all device handles and de-initialize the libusb library.
 */
void
cyusb_close (
        void)
{
    int i;

    //jjustman-2020-07-20 - fix for android usb dev release for re-enumeration
#ifdef __ANDROID__
    cyusb_handle* dev_handle = get_cyusb_handle();
    if(dev_handle) {
        libusb_close(dev_handle);
    }
#else
    for ( i = 0; i < nid; ++i ) {
        libusb_close(cydev[i].handle);
    }
    libusb_free_device_list(list, 1);
#endif

    libusb_exit(NULL);
}

/* cyusb_get_busnumber:
   Get USB bus number on which the specified device is connected.
 */
int
cyusb_get_busnumber (
        cyusb_handle *h)
{
    cyusb_device *tdev = libusb_get_device(h);
    return libusb_get_bus_number( tdev );
}

/* cyusb_get_devaddr:
   Get USB device address assigned to the specified device.
 */
int
cyusb_get_devaddr (
        cyusb_handle *h)
{
    cyusb_device *tdev = libusb_get_device(h);
    return libusb_get_device_address( tdev );
}

/* cyusb_get_max_packet_size:
   Get the max packet size for the specified USB endpoint.
 */
int
cyusb_get_max_packet_size (
        cyusb_handle *h,
        unsigned char endpoint)
{
    cyusb_device *tdev = libusb_get_device(h);
    return ( libusb_get_max_packet_size(tdev, endpoint) );
}

/* cyusb_get_max_iso_packet_size:
   Calculate the maximum packet size which a specific isochronous endpoint is capable of sending or
   receiving in the duration of 1 microframe.
 */
int
cyusb_get_max_iso_packet_size (
        cyusb_handle *h,
        unsigned char endpoint)
{
    cyusb_device *tdev = libusb_get_device(h);
    return ( libusb_get_max_iso_packet_size(tdev, endpoint) );
}

/* cyusb_get_configuration:
   Get the id of the active configuration on the specified USB device.
 */
int
cyusb_get_configuration (
        cyusb_handle *h,
        int *config)
{
    return ( libusb_get_configuration(h, config) );
}

/* cyusb_set_configuration:
   Set the active configuration for the specified USB device.
 */
int
cyusb_set_configuration (
        cyusb_handle *h,
        int config)
{
    return ( libusb_set_configuration(h, config) );
}

/* cyusb_claim_interface:
   Claim the specified USB device interface, so that cyusb/libusb APIs can be used for transfers on the
   corresponding endpoints.
 */
int
cyusb_claim_interface (
        cyusb_handle *h,
        int interface)
{
    return ( libusb_claim_interface(h, interface) );
}

/* cyusb_release_interface:
   Release an interface previously claimed using cyusb_claim_interface.
 */
int
cyusb_release_interface (
        cyusb_handle *h,
        int interface)
{
    return ( libusb_release_interface(h, interface) );
}

/* cyusb_set_interface_alt_setting:
   Select the alternate setting for the specified USB device and interface.
 */
int
cyusb_set_interface_alt_setting (
        cyusb_handle *h,
        int interface,
        int altsetting)
{
    return ( libusb_set_interface_alt_setting(h, interface, altsetting) );
}

/* cyusb_clear_halt:
   Clear the STALL or HALT condition on the specified endpoint.
 */
int
cyusb_clear_halt (
        cyusb_handle *h,
        unsigned char endpoint)
{
    return ( libusb_clear_halt(h, endpoint) );
}

/* cyusb_reset_device:
   Perform a USB port reset to re-initialize the specified USB device.
 */
int
cyusb_reset_device (
        cyusb_handle *h)
{
    return ( libusb_reset_device(h) );
}

/* cyusb_kernel_driver_active:
   Check whether a Kernel driver is currently bound to the specified USB device and interface. The cyusb/libusb
   APIs cannot be used on an interface while a kernel driver is active.
 */
int
cyusb_kernel_driver_active (
        cyusb_handle *h,
        int interface)
{
    return ( libusb_kernel_driver_active(h, interface) );
}

/* cyusb_detach_kernel_driver:
   Detach the kernel driver from the specified USB device and interface, so that the APIs can be used.
 */
int
cyusb_detach_kernel_driver (
        cyusb_handle *h,
        int interface)
{
    return ( libusb_detach_kernel_driver(h, interface) );
}

/* cyusb_attach_kernel_driver:
   Re-attach the kernel driver from the specified USB device and interface.
 */
int
cyusb_attach_kernel_driver (
        cyusb_handle *h,
        int interface)
{
    return ( libusb_attach_kernel_driver(h, interface) );
}

/* cyusb_get_device_descriptor:
   Get the device descriptor for the specified USB device.
 */
int
cyusb_get_device_descriptor (
        cyusb_handle *h,
        struct libusb_device_descriptor *desc)
{
    cyusb_device *tdev = libusb_get_device(h);
    return ( libusb_get_device_descriptor(tdev, desc ) );
}

/* cyusb_get_active_config_descriptor:
   Get the active configuration descriptor for the specified USB device.
 */
int
cyusb_get_active_config_descriptor (
        cyusb_handle *h,
        struct libusb_config_descriptor **config)
{
    cyusb_device *tdev = libusb_get_device(h);
    return ( libusb_get_active_config_descriptor(tdev, config) );
}

/* cyusb_get_config_descriptor:
   Get the configuration descriptor at index config_index, for the specified USB device.
 */
int
cyusb_get_config_descriptor (
        cyusb_handle *h,
        unsigned char config_index,
        struct libusb_config_descriptor **config)
{
    cyusb_device *tdev = libusb_get_device(h);
    return ( libusb_get_config_descriptor(tdev, config_index, config) );
}

/* cyusb_get_config_descriptor_by_value:
   Get the configuration descriptor with value bConfigurationValue for the specified USB device.
 */
int
cyusb_get_config_descriptor_by_value (
        cyusb_handle *h,
        unsigned char bConfigurationValue,
        struct usb_config_descriptor **config)
{
    cyusb_device *tdev = libusb_get_device(h);
    return ( libusb_get_config_descriptor_by_value(tdev, bConfigurationValue,
                                                   (struct libusb_config_descriptor **)config) );
}

/* cyusb_free_config_descriptor:
   Free the configuration descriptor structure after it has been used.
 */
void
cyusb_free_config_descriptor (
        struct libusb_config_descriptor *config)
{
    libusb_free_config_descriptor( (libusb_config_descriptor *)config );
}

/* cyusb_get_string_descriptor_ascii:
   Get the specified USB string descriptor in the format of an ASCII string.
 */
int
cyusb_get_string_descriptor_ascii (
        cyusb_handle *h,
        unsigned char index,
        unsigned char *data,
        int length)
{
    //cyusb_device *tdev = libusb_get_device(h);
    return ( libusb_get_string_descriptor_ascii(h, index, data, length) );
}

/* cyusb_get_descriptor:
   Get a USB descriptor given the type and index.
 */
int
cyusb_get_descriptor (
        cyusb_handle *h,
        unsigned char desc_type,
        unsigned char desc_index,
        unsigned char *data,
        int len)
{
    return ( libusb_get_descriptor(h, desc_type, desc_index, data, len) );
}

/* cyusb_get_string_descriptor:
   Get the USB string descriptor with the specified index.
 */
int
cyusb_get_string_descriptor (
        cyusb_handle *h,
        unsigned char desc_index,
        unsigned short langid,
        unsigned char *data,
        int len)
{
    return ( libusb_get_string_descriptor(h, desc_index, langid, data, len) );
}

/* cyusb_control_transfer:
   Perform the desired USB control transfer (can be read or write based on the bmRequestType value).
 */
int
cyusb_control_transfer (
        cyusb_handle *h,
        unsigned char bmRequestType,
        unsigned char bRequest,
        unsigned short wValue,
        unsigned short wIndex,
        unsigned char *data,
        unsigned short wLength,
        unsigned int timeout)
{
    return ( libusb_control_transfer(h, bmRequestType, bRequest, wValue, wIndex, data, wLength, timeout) );
}

/* cyusb_control_read:
   Perform a read transfer on the USB control endpoint.
 */
int
cyusb_control_read (
        cyusb_handle *h,
        unsigned char bmRequestType,
        unsigned char bRequest,
        unsigned short wValue,
        unsigned short wIndex,
        unsigned char *data,
        unsigned short wLength,
        unsigned int timeout)
{
    /* Set the direction bit to indicate a read transfer. */
    return ( libusb_control_transfer(h, bmRequestType | 0x80, bRequest, wValue, wIndex, data, wLength, timeout) );
}

/* cyusb_control_write:
   Perform a write transfer on the USB control endpoint. This can be a zero byte (no data) transfer as well.
 */
int
cyusb_control_write (
        cyusb_handle *h,
        unsigned char bmRequestType,
        unsigned char bRequest,
        unsigned short wValue,
        unsigned short wIndex,
        unsigned char *data,
        unsigned short wLength,
        unsigned int timeout)
{
    /* Clear the direction bit to indicate a write transfer. */
    return ( libusb_control_transfer(h, bmRequestType & 0x7F, bRequest, wValue, wIndex, data, wLength, timeout) );
}

/* cyusb_bulk_transfer:
   Perform a data transfer on a USB Bulk endpoint.
 */
int
cyusb_bulk_transfer (
        cyusb_handle *h,
        unsigned char endpoint,
        unsigned char *data,
        int length,
        int *transferred,
        int timeout)
{
    printf("invoking libusb_bulk_transfer: %p, h: %p", libusb_bulk_transfer, h);
    return  libusb_bulk_transfer(h, endpoint, data, length, transferred, timeout);
}

/* cyusb_interrupt_transfer:
   Perform a data transfer on a USB interrupt endpoint.
 */
int
cyusb_interrupt_transfer (
        cyusb_handle *h,
        unsigned char endpoint,
        unsigned char *data,
        int length,
        int *transferred,
        unsigned int timeout)
{
    return ( libusb_interrupt_transfer(h, endpoint, data, length, transferred, timeout) );
}

/* cyusb_download_fx2:
   Download firmware to the Cypress FX2/FX2LP device using USB vendor commands.
 */
int
cyusb_download_fx2 (
        cyusb_handle *h,
        char *filename,
        unsigned char vendor_command)
{
    FILE *fp = NULL;
    char buf[256];
    char tbuf1[3];
    char tbuf2[5];
    char tbuf3[3];
    unsigned char reset = 0;
    int r;
    int count = 0;
    unsigned char num_bytes = 0;
    unsigned short address = 0;
    unsigned char *dbuf = NULL;
    int i;

    fp = fopen(filename, "r" );
    tbuf1[2] ='\0';
    tbuf2[4] = '\0';
    tbuf3[2] = '\0';

    /* Place the FX2/FX2LP CPU in reset, so that the vendor commands can be handled by the device. */
    reset = 1;
    r = cyusb_control_transfer(h, 0x40, 0xA0, 0xE600, 0x00, &reset, 0x01, 1000);
    if ( !r ) {
        printf("Error in control_transfer\n");
        return r;
    }
    sleep(1);

    count = 0;

    while ( fgets(buf, 256, fp) != NULL ) {
        if ( buf[8] == '1' )
            break;
        strncpy(tbuf1,buf+1,2);
        num_bytes = strtoul(tbuf1,NULL,16);
        strncpy(tbuf2,buf+3,4);
        address = strtoul(tbuf2,NULL,16);
        dbuf = (unsigned char *)malloc(num_bytes);
        for ( i = 0; i < num_bytes; ++i ) {
            strncpy(tbuf3,&buf[9+i*2],2);
            dbuf[i] = strtoul(tbuf3,NULL,16);
        }

        r = cyusb_control_transfer(h, 0x40, vendor_command, address, 0x00, dbuf, num_bytes, 1000);
        if ( !r ) {
            printf("Error in control_transfer\n");
            free(dbuf);
            return r;
        }
        count += num_bytes;
        free(dbuf);
    }

    printf("Total bytes downloaded = %d\n", count);
    sleep(1);

    /* Bring the CPU out of reset to run the newly loaded firmware. */
    reset = 0;
    r = cyusb_control_transfer(h, 0x40, 0xA0, 0xE600, 0x00, &reset, 0x01, 1000);
    fclose(fp);
    return 0;
}

/* control_transfer:
   Internal function that issues the vendor command that incrementally loads firmware segments to the
   Cypress FX3 device RAM.
 */
//static void
void
control_transfer (
        cyusb_handle *h,
        unsigned int address,
        unsigned char *dbuf,
        int len)
{
    int j;
    int r;
    int b;
    unsigned int *pint;
    int index;

    int balance = len;
    pint = (unsigned int *)dbuf;

    index = 0;
    while ( balance > 0 ) {
        if ( balance > 4096 )
            b = 4096;
        else b = balance;
        r = cyusb_control_transfer (h, 0x40, 0xA0, ( address & 0x0000ffff ), address >> 16,
                                    &dbuf[index], b, 1000);
        if ( r != b ) {
            printf("Error in control_transfer\n");
        }
        address += b ;
        balance -= b;
        index += b;
    }

    /* Update the firmware checksum as the download is being performed. */
    for ( j = 0; j < len/4; ++j )
        checksum += pint[j];
}

/* cyusb_download_fx3:
   Download a firmware binary the Cypress FX3 device RAM.
 */
int
cyusb_download_fx3 (cyusb_handle *h, const char *filename)
{
    printf("in cyusb_download_fx3 with filename: %s", filename);

    unsigned char *fwBuf;
    unsigned int  *data_p;
    unsigned int i, checksum;
    unsigned int address, length;
    int r, index, filesize;

    fwBuf = (unsigned char *)calloc(1, MAX_FWIMG_SIZE);
    if (fwBuf == 0)
    {
#ifdef __ANDROID__
        LOGV("Failed to allocate buffer to store firmware binary");
#else
        printf("Failed to allocate buffer to store firmware binary");
#endif
        return -1;
    }

    // Read the firmware image into the local RAM buffer.
    r = read_firmware_image(filename, fwBuf, NULL, &filesize);
    if (r != 0)
    {
#ifdef __ANDROID__
        LOGV("Failed to read firmware file ");
#else
        printf("Failed to read firmware file ");
#endif
        free(fwBuf);
        return -2;
    }

    // Run through each section of code, and use vendor commands to download them to RAM.
    index = 4;
    checksum = 0;
    while (index < filesize)
    {
        data_p = (unsigned int *)(fwBuf + index);
        length = data_p[0];
        address = data_p[1];
        if (length != 0)
        {
            for (i = 0; i < length; i++)
                checksum += data_p[2 + i];
            r = fx3_ram_write(h, fwBuf + index + 8, address, length * 4);
            if (r != 0)
            {
#ifdef __ANDROID__
                LOGV("Failed to download data to FX3 RAM");
#else
                printf("Failed to download data to FX3 RAM");
#endif
                free(fwBuf);
                return -3;
            }
        }
        else
        {
            if (checksum != data_p[2])
            {
#ifdef __ANDROID__
                LOGV("Ignored error in control transfer");
#else
                printf("Ignored error in control transfer");
#endif
                free(fwBuf);
                return -4;
            }

            r = cyusb_control_transfer(h, 0x40, 0xA0, GET_LSW(address), GET_MSW(address), NULL,
                                       0, VENDORCMD_TIMEOUT);
            if (r != 0)
            {
#ifdef __ANDROID__
                LOGV("Ignored error in control transfer");
#else
                printf("Ignored error in control transfer");
#endif
            }
            break;
        }

        index += (8 + length * 4);
    }

    free(fwBuf);
    return 0;
}

#ifdef __ANDROID__

void cyusb_init(int usbFD)
{
    LOGV("cyusb_init: setting usb_fd to: %d", usbFD);
    usb_fd = usbFD;
}

static libusb_device_handle* get_cyusb_handle() {

    libusb_device_handle *device_handle = NULL;

    int ret = libusb_wrap_sys_device(NULL, usb_fd, &device_handle);
    if(ret || !device_handle) {
        LOGV("unable to open device from fd: %d, ret is: %d, device_handle: %p", fd, ret, device_handle);
        return NULL;
    }

    printf("%s:%d - get_cyusb_handle(), ptr: %p", __FILE__, __LINE__, device_handle);

    return device_handle;
}
#endif
/*[]*/
