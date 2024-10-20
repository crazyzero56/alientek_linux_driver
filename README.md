# alientek_linux_driver
alientek_linux_driver

1. chrdevbase
   * Character driver.
2. gpioled
   * Register a led character driver.
3. beep
   * Beep driver.
4. atomic_gpioled
   * Base on gpioled, append the atomic operation to handle race condation.
5. spinlock
   * Base on atomic_gpioled, repalce atomic operations with spinlock.
6. key
   * register GPIO and APP polling the gpio status.
7. keyirq
   * register GPIO interrupt practice.
8. semaphore_gpioled
   * Trigger gpio led, handling race condation by semaphore.
9.  mutex
   * Trigger gpio led, handling race condation by mutex.
10. timer
   * Gpio twinkling frequency controll by timer.
11. blockio
   * Base on keyirq, use wait queue API instead of spinlock. CPU usage is decrease.
12. platform
   * Practice the platform driver and device module proggramming.
13. i2c
   * Practice I2C driver via coding ap3216c module.
14. SPI
   * Practice SPI driver via coding icm20608 module.
