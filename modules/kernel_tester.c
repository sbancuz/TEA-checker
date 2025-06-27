#include "tester.h"

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "../src/commands.h"

#define _MEM_IMPLEMENTATION
#include "mem.h"

void func(void *);

dev_t dev = 0;
static struct cdev tester_cdev;
static struct class *dev_class;

struct smp_test_data {
  testing_func_t func;
  void *args;
};

extern DECLARE_RESULT;

// This function will be executed on the target CPU
static void __do_run_test_on_cpu(void *data) {
  struct smp_test_data *smp_data = (struct smp_test_data *)data;
  preempt_disable(); // Disable preemption to ensure we stay on this CPU
  local_irq_disable();

  smp_data->func(smp_data->args);

  local_irq_enable();
  preempt_enable(); // Re-enable preemption
}

// Helper to run the test function on a specific CPU
static int run_test(void *args, cpuid_t cpu) {
  struct smp_test_data smp_data;
  smp_data.func = &func;
  smp_data.args = args;

  // Call the function on the specified CPU
  int ret = smp_call_function_single(cpu, __do_run_test_on_cpu, &smp_data, 1);
  if (ret) {
    pr_err("tester: Failed to call function on CPU %d (Error: %d)\n", cpu, ret);
    return ret;
  }
  return 0;
}

static int tester_open(struct inode *inode, struct file *file) { return 0; }
static int tester_release(struct inode *inode, struct file *file) { return 0; }
static long tester_ioctl(struct file *filp, unsigned int cmd,
                         unsigned long arg) {
  __init_alloc();
  long ret = 0;

  RESULT = kmalloc(sizeof(DEFINE_RESULT_STRUCT), GFP_KERNEL);
  if (RESULT == NULL) {
    printk("failed to malloc");
    // TODO: Check to see if theres a better error to return
    ret = -EINVAL;
    goto exit;
  }

  switch ((enum command)cmd) {
  case RUN_FUNCTION: {
    struct run_function_request request;

    if (copy_from_user((void *)&request, (void __user *)arg,
                       sizeof(struct run_function_request))) {

      printk("copy FROM user ");
      ret = -EINVAL;
      goto free;
    }

    run_test(request.args, request.cpu);

    if (copy_to_user(request.ret, RESULT, sizeof(DEFINE_RESULT_STRUCT))) {
      printk("copy TO user ");
      ret = -EINVAL;
      goto free;
    }
  }

  default:
    break;
  }

free:
  kfree(RESULT);
exit:
  __deinit_alloc();

  return ret;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = NULL,
    .write = NULL,
    .open = tester_open,
    .release = tester_release,
    .unlocked_ioctl = tester_ioctl,
};

static int __init tester_reader_init(void) {
  int ret;

  // Allocate device numbers
  ret = alloc_chrdev_region(&dev, 0, 1, "tester_" TEST_NAME_STR);
  if (ret < 0) {
    pr_err("Cannot allocate major number\n");
    return ret;
  }

  // Init and add cdev
  cdev_init(&tester_cdev, &fops);
  ret = cdev_add(&tester_cdev, dev, 1);
  if (ret < 0) {
    pr_err("cdev_add failed\n");
    goto unregister_dev;
  }

  // Create class
  dev_class = class_create("tester_" TEST_NAME_STR "_class");
  if (IS_ERR(dev_class)) {
    pr_err("Failed to create class\n");
    ret = PTR_ERR(dev_class);
    goto del_cdev;
  }

  // Create device
  if (IS_ERR(device_create(dev_class, NULL, dev, NULL,
                           "tester_" TEST_NAME_STR "_device"))) {
    pr_err("Failed to create device\n");
    ret = PTR_ERR(dev_class);
    goto destroy_class;
  }

  pr_info("Kernel Module Inserted Successfully\n");
  return 0;

destroy_class:
  class_destroy(dev_class);

del_cdev:
  cdev_del(&tester_cdev);

unregister_dev:
  unregister_chrdev_region(dev, 1);
  return ret;
}

static void __exit tester_reader_exit(void) {
  device_destroy(dev_class, dev);
  class_destroy(dev_class);
  cdev_del(&tester_cdev);
  unregister_chrdev_region(dev, 1);
  pr_info("Kernel Module Removed Successfully\n");
}

module_init(tester_reader_init);
module_exit(tester_reader_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sbancuz");
MODULE_DESCRIPTION("Tester " TEST_NAME_STR);
