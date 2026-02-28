/**
 * MaahiOS Executive Framework - Common Header
 * 
 * Description:
 *   Defines common structures, states, and patterns for all executives.
 *   Every executive includes this header and follows the patterns defined here.
 * 
 * Executives are Ring 3 services (like Windows Services) that:
 *   - Run continuously via scheduler
 *   - Process requests from applications via message queues
 *   - Make syscalls to kernel managers
 *   - Can be started, stopped, and monitored
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef EXECUTIVE_COMMON_H
#define EXECUTIVE_COMMON_H

#include <stdint.h>

/* NULL definition for freestanding environment */
#ifndef NULL
#define NULL ((void*)0)
#endif

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/* Maximum payload size in request/response messages */
#define EXEC_MSG_MAX_PAYLOAD    256

/* Maximum number of pending requests in queue */
#define EXEC_QUEUE_SIZE         32

/* Maximum name length for executive */
#define EXEC_NAME_MAX           32

/* Executive IDs (well-known) */
#define EXEC_ID_CELL            2   /* Cell Executive (registry) */
#define EXEC_ID_DISK            3   /* Disk Executive (storage) */
#define EXEC_ID_PROCESS         4   /* Process Executive */
#define EXEC_ID_MEMORY          5   /* Memory Executive (heap) */
#define EXEC_ID_LOG             6   /* Log Executive (user-space logging) */
#define EXEC_ID_FS              7   /* Filesystem Executive (file I/O) */
#define EXEC_ID_GUI             8   /* GUI Executive (display/framebuffer) */
#define EXEC_ID_IO              9   /* I/O Executive (keyboard/mouse input) */
#define EXEC_ID_MAX             16  /* Maximum executives */

/*=============================================================================
 * EXECUTIVE STATES
 *===========================================================================*/

typedef enum {
    EXEC_STATE_STOPPED   = 0,   /* Not running */
    EXEC_STATE_STARTING  = 1,   /* Initializing */
    EXEC_STATE_RUNNING   = 2,   /* Processing requests */
    EXEC_STATE_STOPPING  = 3,   /* Shutting down gracefully */
    EXEC_STATE_PAUSED    = 4,   /* Temporarily paused */
    EXEC_STATE_ERROR     = 5    /* Fatal error occurred */
} exec_state_t;

/*=============================================================================
 * PROCESS PRIORITY LEVELS (for scheduler)
 *===========================================================================*/

typedef enum {
    PRIORITY_LOW    = 0,    /* Lower than normal apps */
    PRIORITY_MEDIUM = 1,    /* Same as normal apps (default) */
    PRIORITY_HIGH   = 2     /* Higher than normal apps (executives) */
} exec_priority_t;

/*=============================================================================
 * ERROR CODES
 *===========================================================================*/

#define EXEC_OK                  0
#define EXEC_ERR_INVALID        -1
#define EXEC_ERR_QUEUE_FULL     -2
#define EXEC_ERR_QUEUE_EMPTY    -3
#define EXEC_ERR_NOT_FOUND      -4
#define EXEC_ERR_NO_MEMORY      -5
#define EXEC_ERR_TIMEOUT        -6
#define EXEC_ERR_NOT_RUNNING    -7

/*=============================================================================
 * REQUEST MESSAGE (sent by libraries to executives)
 *===========================================================================*/

typedef struct {
    /* Header */
    uint32_t msg_id;            /* Unique message ID (for response matching) */
    uint32_t sender_pid;        /* PID of the sending process */
    uint32_t exec_id;           /* Which executive this is for */
    uint32_t func_id;           /* Which function to call in the executive */
    uint32_t flags;             /* Request flags (sync/async, etc.) */
    
    /* Payload */
    uint32_t payload_size;      /* Size of payload data */
    uint8_t  payload[EXEC_MSG_MAX_PAYLOAD];  /* Request data */
} exec_request_t;

/*=============================================================================
 * RESPONSE MESSAGE (sent by executives back to callers)
 *===========================================================================*/

typedef struct {
    /* Header */
    uint32_t msg_id;            /* Matches request msg_id */
    int32_t  status;            /* 0 = success, negative = error */
    uint32_t result;            /* Operation result (e.g., handle, count) */
    
    /* Payload */
    uint32_t payload_size;      /* Size of response payload */
    uint8_t  payload[EXEC_MSG_MAX_PAYLOAD];  /* Response data */
} exec_response_t;

/*=============================================================================
 * REQUEST QUEUE (in SHM, one per executive)
 * 
 * Libraries push requests here, executive pops them.
 *===========================================================================*/

typedef struct {
    /* Queue metadata */
    volatile uint32_t head;     /* Next slot to read (consumer) */
    volatile uint32_t tail;     /* Next slot to write (producer) */
    volatile uint32_t count;    /* Number of items in queue */
    
    /* Synchronization */
    volatile uint32_t lock;     /* Spinlock for queue operations */
    
    /* Padding for cache alignment */
    uint32_t reserved[4];
    
    /* Request slots */
    exec_request_t requests[EXEC_QUEUE_SIZE];
} exec_request_queue_t;

/*=============================================================================
 * RESPONSE QUEUE (in SHM, one per executive)
 * 
 * Executive pushes responses here, libraries pop them.
 *===========================================================================*/

typedef struct {
    /* Queue metadata */
    volatile uint32_t head;     /* Next slot to read (consumer) */
    volatile uint32_t tail;     /* Next slot to write (producer) */
    volatile uint32_t count;    /* Number of items in queue */
    
    /* Synchronization */
    volatile uint32_t lock;     /* Spinlock for queue operations */
    
    /* Padding for cache alignment */
    uint32_t reserved[4];
    
    /* Response slots */
    exec_response_t responses[EXEC_QUEUE_SIZE];
} exec_response_queue_t;

/*=============================================================================
 * EXECUTIVE CONTROL BLOCK (ECB)
 * 
 * Stored in SHM, allows sysman to monitor and control executives.
 * Each executive has one ECB.
 *===========================================================================*/

typedef struct {
    /* Identity */
    char name[EXEC_NAME_MAX];   /* Executive name (e.g., "cell_executive") */
    uint32_t exec_id;           /* Executive ID (EXEC_ID_*) */
    uint32_t pid;               /* Process ID */
    exec_state_t state;         /* Current state */
    exec_priority_t priority;   /* Scheduler priority */
    
    /* Queue SHM IDs (for library connection) */
    int32_t request_queue_shm_id;
    int32_t response_queue_shm_id;
    
    /* Statistics */
    uint32_t requests_processed;/* Total requests handled */
    uint32_t requests_failed;   /* Requests that returned error */
    uint32_t start_time;        /* Tick count when started */
    uint32_t uptime;            /* Ticks since started */
    
    /* Control flags (set by sysman) */
    volatile uint32_t stop_requested;   /* 1 = shutdown requested */
    volatile uint32_t pause_requested;  /* 1 = pause requested */
    volatile uint32_t restart_count;    /* Auto-restart counter */
    
    /* Padding */
    uint32_t reserved[8];
} exec_control_block_t;

/*=============================================================================
 * EXECUTIVE REGISTRY (master table of all executives)
 * 
 * Stored in SHM, managed by sysman.
 * Libraries use this to find executive queues.
 *===========================================================================*/

typedef struct {
    uint32_t magic;             /* Magic number for validation */
    uint32_t version;           /* Registry version */
    uint32_t count;             /* Number of registered executives */
    volatile uint32_t lock;     /* Spinlock */
    
    /* Executive control blocks */
    exec_control_block_t executives[EXEC_ID_MAX];
} exec_registry_t;

#define EXEC_REGISTRY_MAGIC     0x45584543  /* "EXEC" */
#define EXEC_REGISTRY_VERSION   1

/*=============================================================================
 * COMMON OPCODES (used by all executives)
 *===========================================================================*/

/* Control opcodes (0-15 reserved for framework) */
#define EXEC_OP_PING            0   /* Health check */
#define EXEC_OP_STATUS          1   /* Get executive status */
#define EXEC_OP_SHUTDOWN        2   /* Request shutdown */
#define EXEC_OP_PAUSE           3   /* Pause processing */
#define EXEC_OP_RESUME          4   /* Resume processing */

/* Executive-specific opcodes start at 16 */
#define EXEC_OP_CUSTOM_BASE     16

/*=============================================================================
 * HELPER MACROS FOR EXECUTIVES
 *===========================================================================*/

/* Check if stop was requested */
#define EXEC_SHOULD_STOP(ecb) ((ecb)->stop_requested != 0)

/* Check if pause was requested */
#define EXEC_SHOULD_PAUSE(ecb) ((ecb)->pause_requested != 0)

/* Set executive state */
#define EXEC_SET_STATE(ecb, s) do { (ecb)->state = (s); } while(0)

/* Increment stats */
#define EXEC_STAT_SUCCESS(ecb) do { (ecb)->requests_processed++; } while(0)
#define EXEC_STAT_FAILURE(ecb) do { (ecb)->requests_failed++; } while(0)

/*=============================================================================
 * STRING HELPER (since we don't have standard library)
 *===========================================================================*/

static inline void exe_str_copy(char *dst, const char *src, int max) {
    int i = 0;
    if (src) {
        while (i < max - 1 && src[i]) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static inline int exe_str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return (*a == *b);
}

static inline int exe_str_len(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

#endif /* EXECUTIVE_COMMON_H */
