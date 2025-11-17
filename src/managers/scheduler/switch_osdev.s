.section .text
.globl switch_to_task

/* 
 * void switch_to_task(task_t *old_task, task_t *new_task)
 * 
 * Based on OSDev wiki multitasking example
 * Saves minimal state to old task's kernel stack
 * Loads state from new task's kernel stack
 * 
 * task_t structure layout:
 * Offset 0: uint32_t esp
 * (Other fields not used by context switch)
 */
switch_to_task:
    /* Save previous task's state */
    /* EAX, ECX, EDX already saved by C calling convention */
    /* EIP already saved by CALL instruction */
    
    push %ebx
    push %esi
    push %edi
    push %ebp
    
    /* Save ESP in old task's TCB */
    mov 20(%esp), %edi              /* edi = old_task (param 1, after 4 pushes + return address) */
    mov %esp, (%edi)                /* old_task->esp = current ESP */
    
    /* Load next task's state */
    mov 24(%esp), %esi              /* esi = new_task (param 2) */
    mov (%esi), %esp                /* ESP = new_task->esp */
    
    /* Restore registers from new task's stack */
    pop %ebp
    pop %edi
    pop %esi
    pop %ebx
    
    /* Return loads new task's EIP from its stack */
    ret
