#ifndef PROBE_PAGING_H_
#   define PROBE_PAGING_H_

#	define PROBE_PROCFS_DIR_NAME "ivhs"
#   define PROBE_PROCFS_FEEDBACK "feedback"
#   define _BUF_LEN 100
#   define ERR_MSG "IVHS not ready!!!"
#   define true_b 1
#   define false_b 0
#   define log_infok(format, args...) do { printk("[ivhs] ");printk(format, ## args ); } while(0)
#ifdef DEBUG
#   define log_debugk(format, args...) do { printk("[ivhs] ");printk(format, ## args ); } while(0)
#else
#	define log_debugk(format, args...)
#endif
/*
 * module setup/teardown functions
 */
extern int init_module ( void );
extern void cleanup_module ( void );

#endif /* PROBE_PAGING_H_ */
