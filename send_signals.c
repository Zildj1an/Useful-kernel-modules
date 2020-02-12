#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h> /* copy_from_user */
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/pid_namespace.h>
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carlos Bilbao");
MODULE_DESCRIPTION("Linux kernel module for testing signal sending");

#define MODULE_NAME     "signal_checker"
#define MAX_SIZE        40

/* My /proc file entry */
static struct proc_dir_entry *my_proc_entry;

/* FUNCTIONS */

void send_signal(int sig_num, struct task_struct* p){

   struct siginfo info;
   int ret = 0;

   memset(&info, 0, sizeof(struct siginfo));
   info.si_signo = sig_num;
   info.si_code = 0;
   info.si_int = 1234;

   ret = send_sig_info(sig_num, &info,  p);
   
   /* Make sure the signal has been captured */
   switch(sig_num){
    case SIGSTOP:
        /* Atomic operations shouldn't have lock acquired */
        printk(KERN_INFO "wait_task_inactive to thread %d in state %ld\n",p->pid,p->state);
        wait_task_inactive(p, p->state);
        break;
    case SIGCONT:
        //printk(KERN_INFO "wake_up_process to thread %d in state %ld\n",p->pid,p->state);
        //wake_up_process(p); Not really needed.
        break;
    default:
        printk(KERN_INFO "Signal %d was only sent...\n",sig_num);
   }

    if (ret < 0) {
           printk(KERN_ALERT "Error sending signal to PID %d\n",p->pid);
    }
}


/* Invoked with echo */
static ssize_t myproc_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {

        char kbuf[MAX_SIZE];
        int pid;
        struct task_struct *target;
        unsigned long signal_n;

        if (len >= MAX_SIZE) return -EFBIG;
        if (copy_from_user(kbuf, buf, len) > 0) return -EINVAL;
        if ((*off > 0)) return 0;

        if (sscanf(kbuf, "sleep %d", &pid) == 1)        signal_n = SIGSTOP;
        else if (sscanf(kbuf, "resume %d", &pid) == 1)  signal_n = SIGCONT;
        else return -EINVAL;

        if (pid < 0) return -EINVAL;

        rcu_read_lock();

        /* To retrieve the task having the PID */
        target = pid_task(find_pid_ns(pid,task_active_pid_ns(current)), PIDTYPE_PID);

        if(!target) {
                rcu_read_unlock();
                return -ESRCH;
        }
        /* Prevent target from going away */
        get_task_struct(target);
        rcu_read_unlock();

        send_signal(signal_n,target);

        put_task_struct(target);

        (*off) += len;

 return len;
}

/* Operations that the module can handle */
const struct file_operations fops = {

        .write = myproc_write,
};

int module_linit(void){

        int ret = 0;

        my_proc_entry = proc_create(MODULE_NAME, 0666, NULL, &fops); //Create proc entry

        if(my_proc_entry == NULL) {
                ret = -ENOMEM;
                printk(KERN_INFO "Couldn't create the /proc entry. \n");
        }

        printk(KERN_INFO "Module %s succesfully charged. \n", MODULE_NAME);

        return ret;
}

void module_lclean(void) {

        remove_proc_entry(MODULE_NAME, NULL);
        printk(KERN_INFO "Module %s disconnected \n", MODULE_NAME);
}

module_init(module_linit);
module_exit(module_lclean);
