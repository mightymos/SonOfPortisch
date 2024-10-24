#ifndef TIMERS_H_
#define TIMERS_H_

extern void InitTimer2_us(uint16_t interval, uint16_t timeout);
extern void InitTimer3_us(uint16_t interval, uint16_t timeout);
extern void InitTimer2_ms(uint16_t interval, uint16_t timeout);
extern void InitTimer3_ms(uint16_t interval, uint16_t timeout);
extern void WaitTimer2Finished(void);
extern void WaitTimer3Finished(void);
extern void StopTimer2(void);
extern void StopTimer3(void);
extern bool IsTimer2Finished(void);
extern bool IsTimer3Finished(void);

#endif