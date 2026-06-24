#include <asm/barrier.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pgtable.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "commands.h"
#include "immintr.h"
#include "types.h"

dev_t dev = 0;
static struct cdev probe_cdev;
static struct class *dev_class;

static pte_t saved_pte;
static pte_t *saved_ptep;

volatile usize *ptr = NULL;

static pte_t *walk_to_pte(unsigned long addr) {
  unsigned long cr3_val;
  pgd_t *pgdp;
  p4d_t *p4dp;
  pud_t *pudp;
  pmd_t *pmdp;

  asm volatile("mov %%cr3, %0" : "=r"(cr3_val));

  /* Bits [11:0] of CR3 are the PCID when CR4.PCIDE=1, or reserved
   * zeroes otherwise.  PAGE_MASK strips them correctly in both cases. */
  pgdp = (pgd_t *)__va(cr3_val & PAGE_MASK) + pgd_index(addr);

  if (pgd_none(*pgdp) || pgd_bad(*pgdp))
    return NULL;
  p4dp = p4d_offset(pgdp, addr);
  if (p4d_none(*p4dp) || p4d_bad(*p4dp))
    return NULL;
  pudp = pud_offset(p4dp, addr);
  if (pud_none(*pudp) || pud_bad(*pudp))
    return NULL;
  pmdp = pmd_offset(pudp, addr);
  if (pmd_none(*pmdp) || pmd_bad(*pmdp))
    return NULL;

  /* Refuse huge pages: a not-present 2 MB entry would break the
   * test's assumption that the victim occupies exactly one 4 KiB
   * TLB slot.                                                          */
  if (pmd_trans_huge(*pmdp)) {
    pr_err("pte_prims: huge page at %lx — not supported\n", addr);
    return NULL;
  }

  return pte_offset_kernel(pmdp, addr);
}

void pte_clear_noflush(volatile char *page) {
  unsigned long addr = (unsigned long)page & PAGE_MASK;
  pte_t *ptep = walk_to_pte(addr);

  if (!ptep) {
    pr_err("pte_clear_noflush: page-table walk failed for %lx\n", addr);
    return;
  }
  if (!(pte_val(*ptep) & _PAGE_PRESENT)) {
    pr_err("pte_clear_noflush: PTE already not-present for %lx\n", addr);
    return;
  }

  saved_pte = *ptep;
  saved_ptep = ptep;

  /* native_set_pte() on x86-64 is WRITE_ONCE(*ptep, pte): a plain
   * 8-byte aligned store.  It does not imply INVLPG or MFENCE.       */
  native_set_pte(ptep, __pte(pte_val(saved_pte) & ~(pteval_t)_PAGE_PRESENT));

  /* Flush the modified PTE cache line to memory.
   * Only the leaf PTE needs flushing — intermediate entries are
   * unchanged and flushing them would evict shared page-table pages
   * from cache for no benefit.                                         */
  asm volatile("clflush (%0)" ::"r"(ptep) : "memory");
  asm volatile("mfence" ::: "memory");
}

void pte_restore_noflush(volatile char *page) {
  if (!saved_ptep) {
    pr_err("pte_restore_noflush: no saved PTE\n");
    return;
  }

  native_set_pte(saved_ptep, saved_pte);

  /* Flush so that the subsequent tlb_flush_page() — which also CLFLUSHes
   * this address — reads the restored value from memory rather than a
   * stale cached copy of the not-present PTE.                          */
  asm volatile("clflush (%0)" ::"r"(saved_ptep) : "memory");
  asm volatile("mfence" ::: "memory");

  saved_ptep = NULL;
}

static int probe_open(struct inode *inode, struct file *file) { return 0; }
static int probe_release(struct inode *inode, struct file *file) { return 0; }
static long probe_ioctl(struct file *filp, unsigned int cmd,
                        unsigned long arg) {

  struct probe_request request;
  unsigned long addr;

  switch ((enum probe_command)cmd) {
  case PROBE_GET:
    if (copy_from_user((void *)&request, (void __user *)arg,
                       sizeof(struct probe_request))) {

      printk("Failed to get the request");
      return -EINVAL;
    }

    // Measure access time
    ktime_t start, end;
    usize value;

    start = __builtin_ia32_rdtsc();
    load(ptr);

    rmb();
    end = __builtin_ia32_rdtsc();

    // Calculate elapsed time in nanoseconds
    u64 elapsed_ns = end - start;

    if (copy_to_user(request.ret, &ptr, sizeof(usize))) {
      printk("Failed to copy result to the user");
      return -EFAULT;
    }

    if (copy_to_user(request.access_time, &elapsed_ns, sizeof(u64))) {
      printk(KERN_ERR "Failed to copy access time to user\n");
      return -EFAULT;
    }

    break;
  case PROBE_CACHE:
    load(ptr);
    load(ptr);
    load(ptr);
    load(ptr);
    load(ptr);
    load(ptr);

    break;
  case PROBE_UNCACHE:
    preempt_disable();
    cache_line_flush(ptr);
    preempt_enable();

    break;

  case PROBE_TLB_FLUSH:
    preempt_disable();
    __flush_tlb_all();
    preempt_enable();

    break;
  case PROBE_TLB_FLUSH_PAGE:
    if (copy_from_user((void *)&request, (void __user *)arg,
                       sizeof(struct probe_request))) {

      printk("Failed to get the request");
      return -EINVAL;
    }

    addr = (unsigned long)request.ret;
    unsigned long cr3_val;

    pgd_t *pgdp;
    p4d_t *p4dp;
    pud_t *pudp;
    pmd_t *pmdp;
    pte_t *ptep;

    asm volatile("mov %%cr3, %0" : "=r"(cr3_val));

    pgdp = (pgd_t *)__va(cr3_val & PAGE_MASK) + pgd_index(addr);
    if (pgd_none(*pgdp) || pgd_bad(*pgdp))
      return -EINVAL;

    p4dp = p4d_offset(pgdp, addr);
    if (p4d_none(*p4dp) || p4d_bad(*p4dp))
      return -EINVAL;

    pudp = pud_offset(p4dp, addr);
    if (pud_none(*pudp) || pud_bad(*pudp))
      return -EINVAL;

    pmdp = pmd_offset(pudp, addr);
    if (pmd_none(*pmdp) || pmd_bad(*pmdp))
      return -EINVAL;

    ptep = pte_offset_kernel(pmdp, addr);
    if (pte_none(*ptep))
      return -EINVAL;

    asm volatile("clflush (%0)" ::"r"(pgdp) : "memory");
    asm volatile("clflush (%0)" ::"r"(p4dp) : "memory");
    asm volatile("clflush (%0)" ::"r"(pudp) : "memory");
    asm volatile("clflush (%0)" ::"r"(pmdp) : "memory");
    asm volatile("clflush (%0)" ::"r"(ptep) : "memory");
    asm volatile("mfence" ::: "memory");

    asm volatile("invlpg (%0)" ::"r"(request.ret) : "memory");
    asm volatile("mfence" ::: "memory");

    break;

  case PROBE_TLB_REMOVE_PTE:
    if (copy_from_user((void *)&request, (void __user *)arg,
                       sizeof(struct probe_request))) {

      printk("Failed to get the request");
      return -EINVAL;
    }

    addr = (unsigned long)request.ret;
    preempt_disable();
    pte_clear_noflush((volatile char *)addr);
    preempt_enable();
    break;
  case PROBE_TLB_RESTORE_PTE:
    if (copy_from_user((void *)&request, (void __user *)arg,
                       sizeof(struct probe_request))) {

      printk("Failed to get the request");
      return -EINVAL;
    }

    addr = (unsigned long)request.ret;
    preempt_disable();
    pte_restore_noflush((volatile char *)addr);
    preempt_enable();
    break;

  default:
    return -EINVAL;
  }
  return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = NULL,
    .write = NULL,
    .open = probe_open,
    .release = probe_release,
    .unlocked_ioctl = probe_ioctl,
};

static int __init probe_init(void) {
  int ret;

  // Allocate device numbers
  ret = alloc_chrdev_region(&dev, 0, 1, "probe");
  if (ret < 0) {
    pr_err("Cannot allocate major number\n");
    return ret;
  }

  // Init and add cdev
  cdev_init(&probe_cdev, &fops);
  ret = cdev_add(&probe_cdev, dev, 1);
  if (ret < 0) {
    pr_err("cdev_add failed\n");
    goto unregister_dev;
  }

  // Create class
  dev_class = class_create("probe_class");
  if (IS_ERR(dev_class)) {
    pr_err("Failed to create class\n");
    ret = PTR_ERR(dev_class);
    goto del_cdev;
  }

  // Create device
  if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "probe_device"))) {
    pr_err("Failed to create device\n");
    ret = PTR_ERR(dev_class);
    goto destroy_class;
  }

  ptr = kmalloc(sizeof(usize), GFP_KERNEL);
  if (ptr == NULL) {
    printk("failed to malloc");
    ret = -ENOMEM;
    goto destroy_class;
  }

  pr_info("Kernel Module Inserted Successfully\n");
  return 0;

destroy_class:
  class_destroy(dev_class);

del_cdev:
  cdev_del(&probe_cdev);

unregister_dev:
  unregister_chrdev_region(dev, 1);
  return ret;
}

static void __exit probe_exit(void) {
  device_destroy(dev_class, dev);
  class_destroy(dev_class);
  cdev_del(&probe_cdev);
  unregister_chrdev_region(dev, 1);
  kfree((void *)ptr);
  pr_info("Kernel Module Removed Successfully\n");
}

module_init(probe_init);
module_exit(probe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("sbancuz");
MODULE_DESCRIPTION("probe");
