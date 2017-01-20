#ifndef _E3_LOG_H
#define _E3_LOG_H


#define E3_ASSERT(condition)  \
do{ \
        if(!(condition)){\
                fprintf(fp_log,"[assert]%s:%d %s() %s\n",__FILE__,__LINE__,__FUNCTION__,#condition); \
                fflush(fp_log); \
                exit(-1); \
        }\
}while(0)


#define E3_LOG(format,...) {\
	fprintf(fp_log,"[log]%s:%d %s() ",__FILE__,__LINE__,__FUNCTION__); \
	fprintf(fp_log,(format),##__VA_ARGS__);} \
	fflush(fp_log);

#define E3_WARN(format,...) {\
	fprintf(fp_log,"[warn]%s:%d %s() ",__FILE__,__LINE__,__FUNCTION__); \
	fprintf(fp_log,(format),##__VA_ARGS__);} \
	fflush(fp_log);

#define E3_ERROR(format,...) {\
	fprintf(fp_log,"[error]%s:%d %s() ",__FILE__,__LINE__,__FUNCTION__); \
	fprintf(fp_log,(format),##__VA_ARGS__);} \
	fflush(fp_log);


#if ! defined(DO_NOT_INCLUDE_FP_LOG)
extern FILE * fp_log;
#endif
//#define LOG_FILE_PATH "/dev/stdout"
#define LOG_FILE_PATH "/root/e3vswitch.log"
#endif
