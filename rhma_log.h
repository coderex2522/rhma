#ifndef RHMA_LOG_H
#define RHMA_LOG_H

enum rhma_log_level{
	RHMA_LOG_LEVEL_ERROR,
	RHMA_LOG_LEVEL_WARN,
	RHMA_LOG_LEVEL_INFO,
	RHMA_LOG_LEVEL_DEBUG,
	RHMA_LOG_LEVEL_TRACE,
	RHMA_LOG_LEVEL_LAST
};

extern enum rhma_log_level global_log_level;

extern void rhma_log_impl(const char *file, unsigned line, const char *func,
					unsigned log_level, const char *fmt, ...);


#define rhma_log(level, fmt, ...)\
	do{\
		if(level < RHMA_LOG_LEVEL_LAST && level<=global_log_level)\
			rhma_log_impl(__FILE__, __LINE__, __func__,\
							level, fmt, ## __VA_ARGS__);\
	}while(0)


#define	ERROR_LOG(fmt, ...)		rhma_log(RHMA_LOG_LEVEL_ERROR, fmt, \
									## __VA_ARGS__)
#define	WARN_LOG(fmt, ...) 		rhma_log(RHMA_LOG_LEVEL_WARN, fmt, \
									## __VA_ARGS__)
#define INFO_LOG(fmt, ...)		rhma_log(RHMA_LOG_LEVEL_INFO, fmt, \
									## __VA_ARGS__)	
#define DEBUG_LOG(fmt, ...)		rhma_log(RHMA_LOG_LEVEL_DEBUG, fmt, \
									## __VA_ARGS__)
#define TRACE_LOG(fmt, ...)		rhma_log(RHMA_LOG_LEVEL_TRACE, fmt,\
									## __VA_ARGS__)
#endif

