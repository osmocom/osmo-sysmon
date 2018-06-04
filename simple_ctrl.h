#pragma once

#include <stdint.h>

struct simple_ctrl_handle;

struct simple_ctrl_handle *simple_ctrl_open(void *ctx, const char *host, uint16_t dport,
					    uint32_t tout_msec);
void simple_ctrl_close(struct simple_ctrl_handle *sch);

char *simple_ctrl_get(struct simple_ctrl_handle *sch, const char *var);
int simple_ctrl_set(struct simple_ctrl_handle *sch, const char *var, const char *val);

