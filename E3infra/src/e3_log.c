#define DO_NOT_INCLUDE_FP_LOG
#include <e3_log.h>
#include <stdio.h>
#include <time.h>

FILE * fp_log=NULL;


__attribute__((constructor)) void e3_log_module_init(void)
{

	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	char s[64];
	strftime(s, sizeof(s), "%c", tm);
	fp_log=fopen(LOG_FILE_PATH,"a+");
	if(!fp_log)
		fp_log=stdout;
	fprintf(fp_log,"\n----------log starts at:%s-----------\n",s);
	fflush(fp_log);
}
