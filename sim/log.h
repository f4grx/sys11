#ifndef __log__h__
#define __log__h__

enum
  {
  SYS_CORE,
  SYS_SCI,
  SYS_GDB,
  };

enum
  {
  CORE_ADMODE,
  CORE_MEM,
  CORE_INST,
  CORE_DBG,
  CORE_ERROR,
  };

void log_init(void);
void log_enable(int system, int subsystem);
void log_msg(int system, int subsystem, const char *fmt, ...);

#endif /* __log__h__ */
