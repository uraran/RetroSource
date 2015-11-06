#ifndef _LOGGING_H
#define _LOGGING_H

#include <android/log.h>

#ifdef __CDT_PARSER__

// Eclipse is having trouble with these macros
#define  LOGI(...)
#define  LOGE(...)
#define  assert(e)

#else

#ifdef NDEBUG

#define  LOGI(...)
#define  LOGE(...)
#ifndef assert
#define  assert(e)
#endif
#define D_PRINTF(...)
#define D_DUMPHEX(buf, len)

#else

#ifndef LOGI
#define  LOGI(...)  	__android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#endif
#ifndef LOGE
#define  LOGE(...)  	__android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#endif

#undef assert
#ifndef assert
#define  assert(e) 		((e) ? (void)0 : __android_log_assert(0,LOG_TAG,"ASSERT: %s(%s:%d) >> %s ",__func__ ,__FILE__, __LINE__, #e))
#endif

#define D_PRINTF(...)	LOGI(__VA_ARGS__)
#define D_DUMPHEX(buf, len)

#endif // NDEBUG

#endif // __CDT_PARSER__

#endif
