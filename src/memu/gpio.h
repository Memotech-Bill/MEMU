/* gpio.h - Routines to access RPi GPIO for joystick emulation. */

#ifndef	H_GPIO
#define	H_GPIO

#define	GPIO_OK			   0
#define	GPIO_ERR_MEM	   -1
#define	GPIO_ERR_ALLOC	   -2
#define	GPIO_ERR_MAP	   -3

#define	GPIO_PIN(i)		   ( 1 << i )

#ifdef __cplusplus
extern "C"
    {
#endif
int gpio_init (void);
void gpio_term (void);
void gpio_input (int iMask);
void gpio_pullup (int iMask);
void gpio_pullnone (int iMask);
int gpio_get (int iMask);
int gpio_revision (void);
#ifdef __cplusplus
    }
#endif

#define	I2C_OK			   0
#define I2C_EOPEN		   -11
#define I2C_EADDR		   -12
#define I2C_EMEM		   -13
#define I2C_EIO			   -14

#ifdef __cplusplus
extern "C"
    {
#endif
int i2c_init (const char *psDev, int iAddr);
void i2c_term (int fd);
int i2c_put (int fd, int iAddr, int iLen, unsigned char *pbData);
int i2c_get (int fd, int iAddr, int iLen, unsigned char *pbData);
#ifdef __cplusplus
    }
#endif

// Generalised Input / Output definitions

#define	 LDEVNAME	 20

struct gio_dev
	{
	char			 sDev[LDEVNAME];   // Device name
	int				 fd;			   // File number
	int				 iAddr;			   // I2C address
	int				 iMask;			   // Bit selection mask
	int				 iData;			   // Data bits
	struct gio_dev * pnext;			   // Next device definition
	};

struct gio_pin
	{
	struct gio_dev * pdev;		 //	Device for this pin
	int				 iMask;		 //	Bit mask for this pin
	int				 iData;		 //	Data for this pin
	};

extern struct gio_dev gdev;

#ifdef __cplusplus
extern "C"
    {
#endif
int gio_init (void);
void gio_term (void);
void gio_clear (void);
void gio_set (struct gio_pin *ppin, int iData);
void gio_input (int nPin, struct gio_pin *ppin, int iData);
void gio_output (int nPin, struct gio_pin *ppin, int iData);
void gio_pullup (int nPin, struct gio_pin *ppin, int iData);
void gio_pullnone (int nPin, struct gio_pin *ppin, int iData);
int gio_get (int nPin, struct gio_pin *ppin);
void gio_put (int nPin, struct gio_pin *ppin, int iData);
#ifdef __cplusplus
    }
#endif

#endif
