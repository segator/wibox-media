#ifndef WIBOX_PROMETHEUS_H
#define WIBOX_PROMETHEUS_H

int prometheus_start(int port);
void prometheus_stop(void);

void prometheus_set_call_active(int active);
void prometheus_set_sip_call_active(int active);
void prometheus_set_video_active(int active);
void prometheus_set_video_enabled(int enabled);
void prometheus_set_ringing(int active);

void prometheus_inc_ring(void);
void prometheus_inc_door_unlock(void);
void prometheus_inc_call_started(void);
void prometheus_inc_video_started(void);
void prometheus_inc_uart_frame(void);
void prometheus_inc_uart_unknown_frame(void);
void prometheus_inc_uart_alarm_report(void);
void prometheus_inc_uart_hangup(void);
void prometheus_inc_uart_stop_ring(void);
void prometheus_inc_uart_reset(void);
void prometheus_inc_uart_push_state(void);
void prometheus_inc_uart_f1(void);

#endif
