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
6. keyirq
   * register GPIO interrupt practice
7. semaphore_gpioled
   * Trigger gpio led, handling race condation by semaphore.
8. mutex
   * Trigger gpio led, handling race condation by mutex.