#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>


/* Structures and types took from:
   https://wx.comake.online/doc/ds82ff82j7jsd9-SSD220/customer/development/arch/arch.html
*/

#define MI_S32 int
#define MI_U64 unsigned long long
#define MI_VIF_CHN int
#define MS_S32 int

typedef enum

{

    E_MI_VIF_CALLBACK_ISR,

    E_MI_VIF_CALLBACK_MAX,

} MI_VIF_CallBackMode_e;

typedef enum

{

    E_MI_VIF_IRQ_FRAMESTART, //frame start irq

    E_MI_VIF_IRQ_FRAMEEND, //frame end irq

    E_MI_VIF_IRQ_LINEHIT, //frame line hit irq

    E_MI_VIF_IRQ_MAX,

} MI_VIF_IrqType_e;


typedef MI_S32 (*MI_VIF_CALLBK_FUNC)(MI_U64 u64Data);


typedef struct MI_VIF_CallBackParam_s

{

    MI_VIF_CallBackMode_e eCallBackMode;

    MI_VIF_IrqType_e eIrqType;

    MI_VIF_CALLBK_FUNC pfnCallBackFunc;

    MI_U64 u64Data;

} MI_VIF_CallBackParam_t;


typedef enum

{

    E_MI_VPE_CALLBACK_ISR,

    E_MI_VPE_CALLBACK_MAX,

} MI_VPE_CallBackMode_e;

typedef enum

{

    E_MI_VPE_IRQ_ISPVSYNC,

    E_MI_VPE_IRQ_ISPFRAMEDONE,

    E_MI_VPE_IRQ_MAX,

} MI_VPE_IrqType_e;

typedef MI_S32 (*MI_VPE_CALLBK_FUNC)(MI_U64 u64Data);

typedef struct MI_VPE_CallBackParam_s

{

    MI_VPE_CallBackMode_e eCallBackMode;

    MI_VPE_IrqType_e eIrqType;

    MI_VPE_CALLBK_FUNC pfnCallBackFunc;

    MI_U64 u64Data;

} MI_VPE_CallBackParam_t;


extern MI_S32 MI_VIF_CallBackTask_Register(MI_VIF_CHN u32VifChn, MI_VIF_CallBackParam_t *pstCallBackParam);
extern MI_S32 MI_VIF_CallBackTask_UnRegister(MI_VIF_CHN u32VifChn, MI_VIF_CallBackParam_t *pstCallBackParam);
extern MI_S32 MI_VPE_CallBackTask_Register(MI_VPE_CallBackParam_t *pstCallBackParam);
extern MI_S32 MI_VPE_CallBackTask_Unregister(MI_VPE_CallBackParam_t *pstCallBackParam);

typedef struct {
    u32 ispframedone_nb;
    u64 vsync_timestamp;
    u64 framestart_timestamp;
    u64 frameend_timestamp;
    u64 ispframedone_timestamp;
} timestamp_buffer_t;

static timestamp_buffer_t buffer1, buffer2;
static timestamp_buffer_t *current_buffer, *read_buffer;
static atomic_t buffer_index;

#define DEBUG

MI_S32 _mi_vif_framestart(MI_U64 u64Data)
{
    static u64 prevts = 0;
    u64 timestamp_ns = ktime_get_ns()  ;
    current_buffer->framestart_timestamp = timestamp_ns;
#ifdef DEBUG
    if (prevts)
    {
        printk(KERN_INFO "framestart   %llu, delta %llu ns\n", timestamp_ns, timestamp_ns - prevts);
    }
    prevts = timestamp_ns;
#endif
    return 0;
}

MI_S32 _mi_vif_frameend(MI_U64 u64Data)
{
    static u64 prevts = 0;
    u64 timestamp_ns = ktime_get_ns()  ;
    current_buffer->frameend_timestamp = timestamp_ns;
#ifdef DEBUG
    if (prevts)
    {
        printk(KERN_INFO "frameend     %llu, delta %llu ns\n", timestamp_ns, timestamp_ns - prevts);
    }
    prevts = timestamp_ns;
#endif
    return 0;
}

MI_S32 _mi_vif_linehit(MI_U64 u64Data)
{
#ifdef DEBUG
    static u64 prevts = 0;
    u64 timestamp_ns = ktime_get_ns()  ;
    if (prevts)
    {
        printk(KERN_INFO "linehit      %llu, delta %llu ns\n", timestamp_ns, timestamp_ns - prevts);
    }
    prevts = timestamp_ns;
#endif
    return 0;
}

MI_S32 _mi_vpe_ispvsync(MI_U64 u64Data)
{
    static u64 prevts = 0;
    u64 timestamp_ns = ktime_get_ns()  ;
    current_buffer->vsync_timestamp = timestamp_ns;
#ifdef DEBUG
    if (prevts)
    {
        printk(KERN_INFO "ispvsync     %llu, delta %llu ns\n", timestamp_ns, timestamp_ns - prevts);
    }
    prevts = timestamp_ns;
#endif
    return 0;
}


MI_S32 _mi_vpe_ispframedone(MI_U64 u64Data)
{
    static u64 prevts = 0;
    u64 timestamp_ns = ktime_get_ns()  ;
    current_buffer->ispframedone_timestamp = timestamp_ns;
    current_buffer->ispframedone_nb = read_buffer->ispframedone_nb + 1;

    // Swap buffers
    if (atomic_xchg(&buffer_index, 1 - atomic_read(&buffer_index)) == 0) {
        read_buffer = &buffer1;
        current_buffer = &buffer2;
    } else {
        read_buffer = &buffer2;
        current_buffer = &buffer1;
    }

#ifdef DEBUG
    if (prevts)
    {
        printk(KERN_INFO "ispframedone %llu, delta %llu ns\n", timestamp_ns, timestamp_ns - prevts);
    }
    prevts = timestamp_ns;
#endif
    return 0;
}

MI_VIF_CallBackParam_t stCallBackParam1 = {
    .eCallBackMode = E_MI_VIF_CALLBACK_ISR,
    .eIrqType = E_MI_VIF_IRQ_FRAMESTART,
    .pfnCallBackFunc = _mi_vif_framestart,
    .u64Data = 1
};
MI_VIF_CallBackParam_t stCallBackParam2 = {
    .eCallBackMode = E_MI_VIF_CALLBACK_ISR,
    .eIrqType = E_MI_VIF_IRQ_FRAMEEND,
    .pfnCallBackFunc = _mi_vif_frameend,
    .u64Data = 2
};
MI_VIF_CallBackParam_t stCallBackParam3 = {
    .eCallBackMode = E_MI_VIF_CALLBACK_ISR,
    .eIrqType = E_MI_VIF_IRQ_LINEHIT,
    .pfnCallBackFunc = _mi_vif_linehit,
    .u64Data = 3
};

MI_VPE_CallBackParam_t stCallBackParam4 = {
    .eCallBackMode = E_MI_VPE_CALLBACK_ISR,
    .eIrqType = E_MI_VPE_IRQ_ISPVSYNC,
    .pfnCallBackFunc = _mi_vpe_ispvsync,
    .u64Data = 4
};

MI_VPE_CallBackParam_t stCallBackParam5 = {
    .eCallBackMode = E_MI_VPE_CALLBACK_ISR,
    .eIrqType = E_MI_VPE_IRQ_ISPFRAMEDONE,
    .pfnCallBackFunc = _mi_vpe_ispframedone,
    .u64Data = 5
};

static MS_S32 _mi_vif_testRegVifCallback(void)
{
    if(MI_VIF_CallBackTask_Register(0,&stCallBackParam1))
    {
        return -1;
    }
    if(MI_VIF_CallBackTask_Register(0,&stCallBackParam2))
    {
        return -1;
    }
    return MI_VIF_CallBackTask_Register(0,&stCallBackParam3);
}

static MS_S32 _mi_vif_testUnRegVifCallback(void)
{

    if (MI_VIF_CallBackTask_UnRegister(0,&stCallBackParam1))
    {
        return -1;
    }
    if (MI_VIF_CallBackTask_UnRegister(0,&stCallBackParam2))
    {
        return -1;
    }
    return MI_VIF_CallBackTask_Register(0,&stCallBackParam3);
}


static MS_S32 _mi_vpe_testRegVpeCallback(void)
{
    if(MI_VPE_CallBackTask_Register(&stCallBackParam4))
    {
        return -1;
    }
    return MI_VPE_CallBackTask_Register(&stCallBackParam5);
}

static MS_S32 _mi_vpe_testUnRegVpeCallback(void)
{

    if (MI_VPE_CallBackTask_Unregister(&stCallBackParam4))
    {
        return -1;
    }
    return MI_VPE_CallBackTask_Unregister(&stCallBackParam5);
}

#define PROC_FILENAME "mi_isr_timestamps"

static ssize_t proc_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    char buffer[256];
    int len = snprintf(buffer, sizeof(buffer),
                   "Frame: %u, VSync: %llu ns, FrameStart: %llu ns, FrameEnd: %llu ns, ISPFrameDone: %llu ns\n",
                   read_buffer->ispframedone_nb,
                   read_buffer->vsync_timestamp,
                   read_buffer->framestart_timestamp,
                   read_buffer->frameend_timestamp,
                   read_buffer->ispframedone_timestamp);
    return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static struct proc_dir_entry *proc_file;
static const struct file_operations proc_file_ops = {
    .owner = THIS_MODULE,
    .read = proc_read,
};

static int __init my_module_init(void) {
    int result;
    atomic_set(&buffer_index, 0);
    current_buffer = &buffer1;
    read_buffer = &buffer2;

    result = _mi_vif_testRegVifCallback();
    if (result) {
        printk(KERN_ERR "Failed to call _mi_vif_testRegVifCallback %d\n", result);
        return result;
    }

    result = _mi_vpe_testRegVpeCallback();
    if (result) {
        printk(KERN_ERR "Failed to call _mi_vpe_testRegVpeCallback %d\n", result);
        return result;
    }

    // Créer l'entrée /proc
    proc_file = proc_create(PROC_FILENAME, 0444, NULL, &proc_file_ops);
    if (!proc_file) {
        _mi_vif_testUnRegVifCallback();
        _mi_vpe_testUnRegVpeCallback();
        printk(KERN_ERR "Failed to create /proc entry\n");
        return -ENOMEM;
    }


    printk(KERN_INFO "Module loaded !\n");
    return 0;
}

static void __exit my_module_exit(void) {
    int result;

    // Supprimer l'entrée /proc
    proc_remove(proc_file);

    result = _mi_vif_testUnRegVifCallback();
    if (result) {
        printk(KERN_ERR "Failed to call _mi_vif_testUnRegVifCallback %i\n", result);
    }

    result = _mi_vpe_testUnRegVpeCallback();
    if (result) {
        printk(KERN_ERR "Failed to call _mi_vpe_testUnRegVpeCallback %i\n", result);
    }


    printk(KERN_INFO "Module unloaded !\n");
}


module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin Le Bihan");
MODULE_DESCRIPTION("Register to sigmastar VIF and VPE callback and print timestamps to /proc/sstarts");