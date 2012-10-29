#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/page-flags.h>
#include "ivhs_feedback.h"


static struct proc_dir_entry *dir_pfs;
static struct proc_dir_entry *ivhs_feedback_pfs;
static char                  ivhs_feedback_buf[_BUF_LEN];
static struct page_state     ivhs_feedback_last;


  void (*__my_pg_state)(struct page_state *pg_state) = (void *) 0xFFFFFF;

static int ivhs_feedback_open(struct inode *inode, struct file *file)
{  return 0; }

static int ivhs_feedback_close(struct inode *inode, struct file *file)
{  return 0; }

static int zero_permissions(struct inode *inode, int op, struct nameidata *foo)
{  return 0; }

static ssize_t ivhs_feedback_write(struct file *file, const char *buffer, size_t count, loff_t * off)
{
  __my_pg_state(&ivhs_feedback_last);
  return count;
}

static ssize_t ivhs_feedback_read(struct file *filp, char *buffer,size_t buffer_length, loff_t * offset)
{
  static unsigned long ivhs_feedback_val = 0;
  static int done = 0;
  struct page_state ivhs_feedback_next;
  int len;
  
  if(done){ done = 0; return 0; }
  done = 1;

  /** someone must be calling us so there have to be mapped pages, unless the page struct hasnt been initialized yet **/
  if(!ivhs_feedback_last.nr_mapped)
    __my_pg_state(&ivhs_feedback_last);
  else
  {
    __my_pg_state(&ivhs_feedback_next);
    ivhs_feedback_val = ivhs_feedback_next.pswpout - ivhs_feedback_last.pswpout;
    ivhs_feedback_last = ivhs_feedback_next;
  }
  len = snprintf ( ivhs_feedback_buf, _BUF_LEN, "%ld", ivhs_feedback_val );
  if ( copy_to_user(buffer, ivhs_feedback_buf, len + 1) ) 
    return -EFAULT;
  log_debugk( "ivhs_feedback_read: %ld\n", ivhs_feedback_val );
  return len;
}

static struct file_operations ivhs_feedback_fops = {
  .read    = ivhs_feedback_read,
  .write   = ivhs_feedback_write,
  .open    = ivhs_feedback_open,
  .release = ivhs_feedback_close,
};

static struct inode_operations proc_iops = {
  .permission = zero_permissions,  /* check for permissions */
};

/*
 * Proc_fs functions
 */

/*********************************************************************
  module/procfs functions
**********************************************************************/

static int configure_proc_entry ( struct proc_dir_entry *entry, struct file_operations *fops, char *fname )
{
  /* create the /proc file */
  entry = create_proc_entry ( fname, 0666, dir_pfs );
  if ( entry == NULL )
  {
    remove_proc_entry ( fname, dir_pfs );
    log_infok ( "configure_proc_entry:error: Could not initialize /proc/%s\n", fname );
    return false_b;
  }
  entry->proc_iops = &proc_iops;
  entry->proc_fops = fops;
  entry->owner = THIS_MODULE;
  entry->mode = S_IFREG | S_IRUGO;
  entry->uid = 0;
  entry->gid = 0;
  log_infok ( "configure_proc_entry:/proc/ivhs/%s created\n", fname );
  return true_b;
}

static int
configure_procfs ( void )
{
  /* create directory */
  dir_pfs = proc_mkdir ( PROBE_PROCFS_DIR_NAME, NULL );
  if ( dir_pfs == NULL )
  {
    remove_proc_entry ( PROBE_PROCFS_DIR_NAME, NULL );
    log_infok( "error: Could not initialize /proc/%s\n", PROBE_PROCFS_DIR_NAME );
    return -ENOMEM;
  }
  dir_pfs->owner = THIS_MODULE;
  if ( !configure_proc_entry ( ivhs_feedback_pfs, &ivhs_feedback_fops, PROBE_PROCFS_FEEDBACK ) )
    goto out_trigger;
  return true_b;
out_trigger:
  remove_proc_entry ( PROBE_PROCFS_FEEDBACK, dir_pfs );
  remove_proc_entry ( PROBE_PROCFS_DIR_NAME, NULL );
  return false_b;
}

void cleanup_module ( void )
{
  log_infok( "cleanup_module:cleaning up /proc/ivhs entries...\n" );
  remove_proc_entry ( PROBE_PROCFS_FEEDBACK, dir_pfs );
  remove_proc_entry ( PROBE_PROCFS_DIR_NAME, NULL );
}

int init_module ( void )
{
  log_infok( "init_module:setting up /proc/ivhs entries...\n" );
  if ( !configure_procfs ( ) )
    return -ENOMEM;
  log_infok( "ivhs ready... \n" );
  return 0;
}

MODULE_AUTHOR ( "Chris Grzegorczyk" );
MODULE_DESCRIPTION ( "Isla Vista Heap Sizing: Simple allocstall feedback to guide heap sizing." );
MODULE_LICENSE ( "GPL" );
