.section .text
.globl switch_to_task

/* 
 * void switch_to_task(task_t *old_task, task_t *new_task)
 * 
 * Simple context switch: just save/restore ESP, EBP, and return address
 * 
 * task_t structure layout (offsets):
 * 0:  esp
 * 4:  ebp
 * 8:  eip (not used in this simple version)
 */
switch_to_task:
    /* Get task pointers from arguments */
    mov 4(%esp), %eax      /* eax = old_task */
    mov 8(%esp), %edx      /* edx = new_task */
    
    /* Save old task's ESP and EBP */
    mov %esp, 0(%eax)      /* old_task->esp = esp */
    mov %ebp, 4(%eax)      /* old_task->ebp = ebp */
    
    /* Load new task's ESP and EBP */
    mov 0(%edx), %esp      /* esp = new_task->esp */
    mov 4(%edx), %ebp      /* ebp = new_task->ebp */
    
    /* Return (will pop return address from new task's stack) */
    ret
