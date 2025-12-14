/*
 * FreeRTOS Concurrent Web Server Simulator for RISC-V
 * Demonstrates OS Concepts: Scheduling, Memory Management, Synchronization, IPC
 * Based on OSTEP Chapters 26, 27, 30, 31 (Concurrency & Threads)
  
 */

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <semphr.h>
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * OS CONCEPT 1: MEMORY MANAGEMENT
 * ============================================================================
 * Request structure stored in dynamically allocated queue
 */
typedef struct {
    int requestId;
    char method[8];      // GET, POST, etc.
    char url[64];        // Requested URL
    int processingTime;  // Simulated processing time (ms)
} Request;

/* ============================================================================
 * GLOBAL VARIABLES - OS CONCEPT 3: SYNCHRONIZATION
 * ============================================================================
 */
QueueHandle_t requestQueue;           // OS CONCEPT 4: IPC - Message Queue
SemaphoreHandle_t statsMutex;         // OS CONCEPT 3: Mutex for shared data
SemaphoreHandle_t workerSemaphore;    // OS CONCEPT 3: Counting semaphore

// Shared statistics (protected by mutex)
typedef struct {
    int totalRequests;
    int processedRequests;
    int queuedRequests;
    int droppedRequests;
} ServerStats;

ServerStats stats = {0, 0, 0, 0};

// Task handles for monitoring
TaskHandle_t masterHandle = NULL;
TaskHandle_t worker1Handle = NULL;
TaskHandle_t worker2Handle = NULL;
TaskHandle_t statsHandle = NULL;

/* ============================================================================
 * OS CONCEPT 1: SCHEDULING - PRIORITY DEFINITIONS
 * ============================================================================
 * Priority levels:
 * - High (3): Master task - generates requests quickly
 * - Medium (2): Worker tasks - process requests  
 * - Low (1): Stats task - monitoring only
 */
#define MASTER_PRIORITY    3
#define WORKER_PRIORITY    2
#define STATS_PRIORITY     1

/* Configuration */
#define QUEUE_SIZE         10
#define TOTAL_REQUESTS     20
#define MAX_WORKERS        2
#define WORKER_TIMEOUT_MS  100

/* ============================================================================
 * MASTER TASK - PRODUCER
 * ============================================================================
 * OS CONCEPTS DEMONSTRATED:
 * - HIGH PRIORITY SCHEDULING (preempts lower priority tasks)
 * - IPC via Queue Send
 * - Memory allocation for requests
 */
void MasterTask(void *pvParameters)
{
    Request req;
    int requestCounter = 0;
    
    printf("\n=== MASTER TASK STARTED (Priority: %d) ===\n", MASTER_PRIORITY);
    printf("Generating %d HTTP requests...\n\n", TOTAL_REQUESTS);
    
    // Simulate different types of HTTP requests
    const char* urls[] = {"/index.html", "/api/data", "/images/logo.png", "/contact", "/about"};
    const char* methods[] = {"GET", "POST"};
    
    while (requestCounter < TOTAL_REQUESTS) {
        // Create new request
        req.requestId = requestCounter + 1;
        strcpy(req.method, methods[requestCounter % 2]);
        strcpy(req.url, urls[requestCounter % 5]);
        req.processingTime = 100 + (requestCounter % 5) * 50; // 100-350ms
        
        printf("[MASTER] Generated Request #%d: %s %s (Processing: %dms)\n",
               req.requestId, req.method, req.url, req.processingTime);
        
        // OS CONCEPT 4: IPC - Send to queue with timeout
        if (xQueueSend(requestQueue, &req, pdMS_TO_TICKS(500)) == pdPASS) {
            // OS CONCEPT 3: SYNCHRONIZATION - Update shared stats
            xSemaphoreTake(statsMutex, portMAX_DELAY);
            stats.totalRequests++;
            stats.queuedRequests++;
            xSemaphoreGive(statsMutex);
            
            printf("[MASTER] Request #%d queued successfully\n", req.requestId);
        } else {
            // Queue full - request dropped
            xSemaphoreTake(statsMutex, portMAX_DELAY);
            stats.droppedRequests++;
            xSemaphoreGive(statsMutex);
            
            printf("[MASTER] Request #%d DROPPED (queue full)\n", req.requestId);
        }
        
        requestCounter++;
        
        // Delay to simulate request arrival rate
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    printf("\n[MASTER] All requests generated. Master task finishing.\n");
    
    // Signal completion
    vTaskDelay(pdMS_TO_TICKS(5000)); // Wait for workers to finish
    
    printf("\n=== FINAL SERVER STATISTICS ===\n");
    printf("Total Requests: %d\n", stats.totalRequests);
    printf("Processed: %d\n", stats.processedRequests);
    printf("Dropped: %d\n", stats.droppedRequests);
    printf("Success Rate: %d%%\n", 
           stats.totalRequests > 0 ? (stats.processedRequests * 100) / stats.totalRequests : 0);
    
    vTaskDelete(NULL);
}

/* ============================================================================
 * WORKER TASK - CONSUMER
 * ============================================================================
 * OS CONCEPTS DEMONSTRATED:
 * - MEDIUM PRIORITY SCHEDULING (preempted by Master, preempts Stats)
 * - ROUND-ROBIN scheduling (multiple workers at same priority)
 * - IPC via Queue Receive
 * - DEADLOCK PREVENTION using timeouts
 * - CONTEXT SWITCHING (voluntary via vTaskDelay)
 */
void WorkerTask(void *pvParameters)
{
    int workerID = (int)pvParameters;
    Request req;
    
    printf("[WORKER-%d] Started (Priority: %d)\n", workerID, WORKER_PRIORITY);
    
    while (1) {
        // OS CONCEPT 4: IPC - Receive from queue with timeout
        // OS CONCEPT 5: DEADLOCK PREVENTION - timeout prevents infinite blocking
        if (xQueueReceive(requestQueue, &req, pdMS_TO_TICKS(WORKER_TIMEOUT_MS)) == pdPASS) {
            
            printf("[WORKER-%d] 🔧 Processing Request #%d: %s %s\n",
                   workerID, req.requestId, req.method, req.url);
            
            // OS CONCEPT 3: SYNCHRONIZATION - Update shared counter
            xSemaphoreTake(statsMutex, portMAX_DELAY);
            stats.queuedRequests--;
            xSemaphoreGive(statsMutex);
            
            // Simulate request processing
            // OS CONCEPT 1: CONTEXT SWITCHING - Voluntary yield during I/O
            vTaskDelay(pdMS_TO_TICKS(req.processingTime));
            
            // OS CONCEPT 3: SYNCHRONIZATION - Mutex protects shared data
            // OS CONCEPT 5: DEADLOCK PREVENTION - Use timeout
            if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(1000)) == pdPASS) {
                stats.processedRequests++;
                xSemaphoreGive(statsMutex);
                
                printf("[WORKER-%d] Completed Request #%d (Total processed: %d)\n",
                       workerID, req.requestId, stats.processedRequests);
            } else {
                printf("[WORKER-%d] Mutex timeout for Request #%d\n", workerID, req.requestId);
            }
            
        } else {
            // Queue empty or timeout - worker idle
            // This demonstrates CONTEXT SWITCHING when no work available
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

/* ============================================================================
 * STATISTICS MONITOR TASK
 * ============================================================================
 * OS CONCEPTS DEMONSTRATED:
 * - LOW PRIORITY SCHEDULING (runs only when higher priority tasks yield)
 * - MEMORY MANAGEMENT monitoring (heap usage)
 * - Task state monitoring
 */
void StatsTask(void *pvParameters)
{
    printf("[STATS] Monitor started (Priority: %d)\n\n", STATS_PRIORITY);
    
    vTaskDelay(pdMS_TO_TICKS(1000)); // Initial delay
    
    while (1) {
        printf("\n╔════════════════════════════════════════════════╗\n");
        printf("║         REAL-TIME SERVER STATISTICS           ║\n");
        printf("╠════════════════════════════════════════════════╣\n");
        
        // OS CONCEPT 3: SYNCHRONIZATION - Read shared data safely
        xSemaphoreTake(statsMutex, portMAX_DELAY);
        printf("║ Total Requests:     %3d                       ║\n", stats.totalRequests);
        printf("║ Processed:          %3d                       ║\n", stats.processedRequests);
        printf("║ In Queue:           %3d                       ║\n", stats.queuedRequests);
        printf("║ Dropped:            %3d                       ║\n", stats.droppedRequests);
        xSemaphoreGive(statsMutex);
        
        printf("╠════════════════════════════════════════════════╣\n");
        
        // OS CONCEPT 2: MEMORY MANAGEMENT - Heap monitoring
        printf("║ Free Heap:          %6d bytes              ║\n", xPortGetFreeHeapSize());
        printf("║ Min Free Heap:      %6d bytes              ║\n", xPortGetMinimumEverFreeHeapSize());
        
        printf("╠════════════════════════════════════════════════╣\n");
        
        // OS CONCEPT 1: SCHEDULING - Task state monitoring
        printf("║ Queue Usage:        %2d/%2d slots              ║\n", 
               uxQueueMessagesWaiting(requestQueue), QUEUE_SIZE);
        
        if (masterHandle != NULL) {
            eTaskState state = eTaskGetState(masterHandle);
            const char* stateStr[] = {"Running", "Ready", "Blocked", "Suspended", "Deleted"};
            printf("║ Master State:       %-10s               ║\n", stateStr[state]);
        }
        
        printf("╚════════════════════════════════════════════════╝\n");
        
        // OS CONCEPT 1: CONTEXT SWITCHING - Low priority task yields
        vTaskDelay(pdMS_TO_TICKS(2000)); // Update every 2 seconds
    }
}

/* ============================================================================
 * MAIN APPLICATION ENTRY POINT
 * ============================================================================
 */
void main_blinky(void)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  FreeRTOS Concurrent Web Server Simulator - RISC-V      ║\n");
    printf("║  Demonstrating OS Concepts in Action                    ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("OS CONCEPTS DEMONSTRATED:\n");
    printf("  1. SCHEDULING: Priority-based preemptive scheduling\n");
    printf("     - Master (High), Workers (Medium), Stats (Low)\n");
    printf("  2. MEMORY MANAGEMENT: Dynamic allocation & heap monitoring\n");
    printf("  3. SYNCHRONIZATION: Mutex protecting shared statistics\n");
    printf("  4. IPC: Message queue for request passing\n");
    printf("  5. DEADLOCK PREVENTION: Timeouts on blocking operations\n");
    printf("  6. CONTEXT SWITCHING: Scheduler manages task execution\n");
    printf("\n");
    
    /* OS CONCEPT 2: MEMORY MANAGEMENT - Dynamic allocation */
    printf("Initializing system resources...\n");
    
    // Create request queue (IPC mechanism)
    requestQueue = xQueueCreate(QUEUE_SIZE, sizeof(Request));
    if (requestQueue == NULL) {
        printf("ERROR: Failed to create request queue!\n");
        return;
    }
    printf("✓ Request queue created (size: %d)\n", QUEUE_SIZE);
    
    // Create mutex for shared statistics
    statsMutex = xSemaphoreCreateMutex();
    if (statsMutex == NULL) {
        printf("ERROR: Failed to create mutex!\n");
        return;
    }
    printf("✓ Statistics mutex created\n");
    
    // Create counting semaphore for worker pool management
    workerSemaphore = xSemaphoreCreateCounting(MAX_WORKERS, MAX_WORKERS);
    if (workerSemaphore == NULL) {
        printf("ERROR: Failed to create worker semaphore!\n");
        return;
    }
    printf("✓ Worker semaphore created (max: %d)\n", MAX_WORKERS);
    
    printf("\n");
    
    /* OS CONCEPT 1: SCHEDULING - Create tasks with different priorities */
    
    // HIGH PRIORITY: Master task (Producer)
    if (xTaskCreate(MasterTask, "Master", 
                    configMINIMAL_STACK_SIZE * 2, 
                    NULL, 
                    MASTER_PRIORITY, 
                    &masterHandle) != pdPASS) {
        printf("ERROR: Failed to create Master task!\n");
        return;
    }
    
    // MEDIUM PRIORITY: Worker tasks (Consumers) - Round-robin scheduling
    if (xTaskCreate(WorkerTask, "Worker-1", 
                    configMINIMAL_STACK_SIZE * 2, 
                    (void*)1, 
                    WORKER_PRIORITY, 
                    &worker1Handle) != pdPASS) {
        printf("ERROR: Failed to create Worker-1 task!\n");
        return;
    }
    
    if (xTaskCreate(WorkerTask, "Worker-2", 
                    configMINIMAL_STACK_SIZE * 2, 
                    (void*)2, 
                    WORKER_PRIORITY, 
                    &worker2Handle) != pdPASS) {
        printf("ERROR: Failed to create Worker-2 task!\n");
        return;
    }
    
    // LOW PRIORITY: Statistics monitor
    if (xTaskCreate(StatsTask, "Stats", 
                    configMINIMAL_STACK_SIZE * 2, 
                    NULL, 
                    STATS_PRIORITY, 
                    &statsHandle) != pdPASS) {
        printf("ERROR: Failed to create Stats task!\n");
        return;
    }
    
    printf("✓ All tasks created successfully\n");
    printf("  - Master Task (Priority %d)\n", MASTER_PRIORITY);
    printf("  - Worker-1 Task (Priority %d)\n", WORKER_PRIORITY);
    printf("  - Worker-2 Task (Priority %d)\n", WORKER_PRIORITY);
    printf("  - Stats Task (Priority %d)\n", STATS_PRIORITY);
    printf("\n");
    
    printf("Starting FreeRTOS scheduler...\n");
    printf("════════════════════════════════════════════════════════════\n\n");
    
    /* Start the FreeRTOS scheduler - OS CONCEPT 1: SCHEDULING */
    vTaskStartScheduler();
    
    /* Should never reach here */
    printf("ERROR: Scheduler failed to start!\n");
    for(;;);
}
