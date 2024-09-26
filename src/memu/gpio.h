/* gpio.h - Routines to access RPi GPIO for joystick emulation. */

#ifndef	H_GPIO
#define	H_GPIO

#include <stdint.h>

#define	GPIO_OK			   0
#define	GPIO_ERR_MEM	   -1
#define	GPIO_ERR_ALLOC	   -2
#define	GPIO_ERR_MAP	   -3

#define	GPIO_PIN(i)		   ( 1 << i )

#define	I2C_OK			   0
#define I2C_EOPEN		   -11
#define I2C_EADDR		   -12
#define I2C_EMEM		   -13
#define I2C_EIO			   -14

// Generalised Input / Output definitions

#define	 LDEVNAME	 20

typedef enum {
#if HAVE_HW_GPIO
    gio_gpio,
#endif
#if HAVE_HW_MCP23017
    gio_xio,
#endif
    } gio_type;

struct gio_dev
	{
    gio_type         type;             // Device type
	char			 sDev[LDEVNAME];   // Device name
	int				 fd;			   // File number
	int				 iAddr;			   // I2C address
    uint32_t         iPins;            // 1 = Active pin, 0 = Inactive pin
    uint32_t         iDirn;            // Direction: 0 = Input, 1 = Output
    uint32_t         iPullUp;          // Pull Up: 0 = Disabled, 1 = Enabled
    uint32_t         iPullDn;          // Pull Down: 0 = Disabled, 1 = Enabled
	uint32_t		 iMask;			   // Bit selection mask
	uint32_t		 iData;			   // Data bits
	struct gio_dev * pnext;			   // Next device definition
	};

struct gio_pin
	{
	struct gio_dev * pdev;		 //	Device for this pin set
	int				 iMask;		 //	Bit mask for this pin set
	int				 iData;		 //	Data for this pin set
	};

extern struct gio_dev *gdev;

#ifdef __cplusplus
extern "C"
    {
#endif
int gpio_init (struct gio_dev *pdev);
void gpio_term (struct gio_dev *pdev);
void gpio_input (struct gio_dev *pdev, uint32_t iMask);
void gpio_pullup (struct gio_dev *pdev, uint32_t iMask);
void gpio_pullnone (struct gio_dev *pdev, uint32_t iMask);
uint32_t gpio_get (struct gio_dev *pdev, uint32_t iMask);
int gpio_revision (void);
int i2c_init (const char *psDev, int iAddr);
void i2c_term (int fd);
int i2c_put (int fd, int iAddr, int iLen, unsigned char *pbData);
int i2c_get (int fd, int iAddr, int iLen, unsigned char *pbData);
int gio_init (void);
void gio_term (void);
void gio_clear (void);
void gio_set (struct gio_pin *ppin, uint32_t iData);
void gio_input (int nPin, struct gio_pin *ppin, uint32_t iData);
void gio_output (int nPin, struct gio_pin *ppin, uint32_t iData);
void gio_pullup (int nPin, struct gio_pin *ppin, uint32_t iData);
void gio_pullnone (int nPin, struct gio_pin *ppin, uint32_t iData);
uint32_t gio_get (int nPin, struct gio_pin *ppin);
void gio_put (int nPin, struct gio_pin *ppin, uint32_t iData);
#ifdef __cplusplus
    }
#endif

#endif
