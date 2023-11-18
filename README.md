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
