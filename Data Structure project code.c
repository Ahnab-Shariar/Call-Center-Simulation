#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define MAX_CALL_DURATION 200
#define MAX_AGENT_COUNT 5
#define MAX_CALLER_NAME 50
#define MAX_PHONE_NUMBER 15
#define DATA_FILE "call_center_data.dat"

typedef enum
{
    PRIORITY_VIP = 0,
    PRIORITY_HIGH = 1,
    PRIORITY_MEDIUM = 2,
    PRIORITY_LOW = 3
} Priority;

typedef struct Call
{
    int id;
    Priority priority;
    int duration;
    char caller_name[MAX_CALLER_NAME];
    char phone_number[MAX_PHONE_NUMBER];
    time_t start_time;
    struct Call* next;
} Call;

typedef struct
{
    Call* front;
    Call* rear;
} Queue;

typedef struct
{
    int id;
    bool busy;
    pthread_t thread;
    int current_call_id;
    int total_calls_handled;
    int total_time_spent;
    char current_caller[MAX_CALLER_NAME];
} Agent;

void assignCall();

Agent* agents;
int agent_count;
Queue callQueue = {NULL, NULL};
int call_id_counter = 1;
pthread_mutex_t queue_lock;

// Improved call creation with caller details
Call* createCall(Priority priority, int duration, const char* caller_name, const char* phone_number)
{
    Call* newCall = (Call*)malloc(sizeof(Call));
    newCall->id = call_id_counter++;
    newCall->priority = priority;
    newCall->duration = duration;
    strncpy(newCall->caller_name, caller_name, MAX_CALLER_NAME);
    strncpy(newCall->phone_number, phone_number, MAX_PHONE_NUMBER);
    newCall->start_time = time(NULL);
    newCall->next = NULL;
    return newCall;
}

void enqueue(Call* newCall)
{
    pthread_mutex_lock(&queue_lock);

    if (!callQueue.front || newCall->priority < callQueue.front->priority)
    {
        newCall->next = callQueue.front;
        callQueue.front = newCall;
        if (!callQueue.rear) callQueue.rear = newCall;
    }
    else
    {
        Call* temp = callQueue.front;
        while (temp->next && temp->next->priority <= newCall->priority)
            temp = temp->next;
        newCall->next = temp->next;
        temp->next = newCall;
        if (!newCall->next) callQueue.rear = newCall;
    }

    pthread_mutex_unlock(&queue_lock);
}

Call* dequeue()
{
    pthread_mutex_lock(&queue_lock);
    if (!callQueue.front)
    {
        pthread_mutex_unlock(&queue_lock);
        return NULL;
    }
    Call* temp = callQueue.front;
    callQueue.front = callQueue.front->next;
    if (!callQueue.front) callQueue.rear = NULL;
    pthread_mutex_unlock(&queue_lock);
    return temp;
}

void* handleCall(void* arg)
{
    int agent_id = (int)arg;
    free(arg);
    Call* call = dequeue();

    if (!call)
    {
        agents[agent_id].busy = false;
        return NULL;
    }

    agents[agent_id].busy = true;
    agents[agent_id].current_call_id = call->id;
    strncpy(agents[agent_id].current_caller, call->caller_name, MAX_CALLER_NAME);

    printf("\nAgent %d handling Call ID %d\n", agents[agent_id].id, call->id);
    printf("Caller: %s (%s)\n", call->caller_name, call->phone_number);
    printf("Priority: %d, Duration: %d seconds\n", call->priority, call->duration);

    time_t start_time = time(NULL);
    while ((time(NULL) - start_time) < call->duration)
    {
        if (!agents[agent_id].busy)   // Check if agent was manually released
        {
            printf("Call ID %d was terminated manually!\n", call->id);
            free(call);
            return NULL;
        }
        sleep(1);
    }

    agents[agent_id].total_calls_handled++;
    agents[agent_id].total_time_spent += call->duration;

    printf("\nCall ID %d completed by Agent %d\n", call->id, agents[agent_id].id);
    printf("Call Duration: %ld seconds\n", time(NULL) - call->start_time);

    free(call);
    agents[agent_id].busy = false;
    agents[agent_id].current_call_id = -1;
    agents[agent_id].current_caller[0] = '\0';

    assignCall();
    return NULL;
}

void assignCall()
{
    pthread_mutex_lock(&queue_lock);
    for (int i = 0; i < agent_count; i++)
    {
        if (!agents[i].busy && callQueue.front)
        {
            int* agent_id = malloc(sizeof(int));
            *agent_id = i;
            pthread_create(&agents[i].thread, NULL, handleCall, agent_id);
            agents[i].busy = true;
        }
    }
    pthread_mutex_unlock(&queue_lock);
}

void releaseAgent(int agent_id)
{
    pthread_mutex_lock(&queue_lock);
    if (agent_id < 1 || agent_id > agent_count)
    {
        printf("Invalid Agent ID!\n");
        pthread_mutex_unlock(&queue_lock);
        return;
    }
    agent_id--;

    if (agents[agent_id].busy)
    {
        printf("Agent %d released from Call ID %d\n", agents[agent_id].id, agents[agent_id].current_call_id);
        agents[agent_id].busy = false; // This will terminate the current call
    }
    else
    {
        printf("Agent %d is already available\n", agent_id + 1);
    }
    pthread_mutex_unlock(&queue_lock);
}

void displayQueue()
{
    pthread_mutex_lock(&queue_lock);
    if (!callQueue.front)
    {
        printf("\nQueue is empty.\n");
    }
    else
    {
        printf("\nCurrent Call Queue:\n");
        printf("ID\t\tPriority\t\tCaller\t\tPhone\t\tDuration\n");
        Call* temp = callQueue.front;
        while (temp)
        {
            printf("%d\t\t%d\t\t\t%s\t\t%s\t%d\n",
                   temp->id, temp->priority, temp->caller_name,
                   temp->phone_number, temp->duration);
            temp = temp->next;
        }
    }
    pthread_mutex_unlock(&queue_lock);
}

void displayAgentStatus()
{
    pthread_mutex_lock(&queue_lock);
    printf("\nAgent Status:\n");
    printf("ID\tStatus\t\tCurrent Call\tCaller\t\tTotal Calls\tTotal Time\n");
    for (int i = 0; i < agent_count; i++)
    {
        printf("%d\t%s\t",
               agents[i].id,
               agents[i].busy ? "Busy" : "Available");

        if (agents[i].busy)
        {
            printf("%d\t\t%s", agents[i].current_call_id, agents[i].current_caller);
        }
        else
        {
            printf("-\t\t-");
        }

        printf("\t%d\t\t%d sec\n",
               agents[i].total_calls_handled,
               agents[i].total_time_spent);
    }
    pthread_mutex_unlock(&queue_lock);
}

void saveData()
{
    FILE* fp = fopen(DATA_FILE, "wb");
    if (!fp)
    {
        perror("Error saving data");
        return;
    }

    // Save call queue
    Call* temp = callQueue.front;
    while (temp)
    {
        fwrite(temp, sizeof(Call), 1, fp);
        temp = temp->next;
    }

    // Save agents data
    for (int i = 0; i < agent_count; i++)
    {
        fwrite(&agents[i], sizeof(Agent), 1, fp);
    }

    fclose(fp);
    printf("Data saved successfully!\n");
}

void loadData()
{
    FILE* fp = fopen(DATA_FILE, "rb");
    if (!fp)
    {
        printf("No previous data found\n");
        return;
    }

    // Clear existing queue
    while (callQueue.front)
    {
        Call* temp = callQueue.front;
        callQueue.front = callQueue.front->next;
        free(temp);
    }
    callQueue.rear = NULL;

    // Load call queue
    Call call;
    while (fread(&call, sizeof(Call), 1, fp) == 1)
    {
        Call* newCall = createCall(call.priority, call.duration, call.caller_name, call.phone_number);
        newCall->id = call.id;
        newCall->start_time = call.start_time;
        enqueue(newCall);
    }

    // Load agents data
    for (int i = 0; i < agent_count; i++)
    {
        fread(&agents[i], sizeof(Agent), 1, fp);
    }

    fclose(fp);
    printf("Data loaded successfully!\n");
}

int main()
{
    pthread_mutex_init(&queue_lock, NULL);

    printf("Enter the number of agents (max %d): ", MAX_AGENT_COUNT);
    scanf("%d", &agent_count);
    if (agent_count > MAX_AGENT_COUNT) agent_count = MAX_AGENT_COUNT;

    agents = (Agent*)malloc(agent_count * sizeof(Agent));
    for (int i = 0; i < agent_count; i++)
    {
        agents[i].id = i + 1;
        agents[i].busy = false;
        agents[i].current_call_id = -1;
        agents[i].total_calls_handled = 0;
        agents[i].total_time_spent = 0;
        agents[i].current_caller[0] = '\0';
    }

    loadData();

    int choice, priority, duration, agent_id;
    char caller_name[MAX_CALLER_NAME];
    char phone_number[MAX_PHONE_NUMBER];

    while (1)
    {
        printf("\nCall Center Simulation:\n");
        printf("1. Add Call\n");
        printf("2. Assign Call\n");
        printf("3. Release Agent\n");
        printf("4. Display Queue\n");
        printf("5. Display Agent Status\n");
        printf("6. Save Data\n");
        printf("7. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &choice);

        switch (choice)
        {
        case 1:
            printf("Enter Call Priority (0-VIP, 1-High, 2-Medium, 3-Low): ");
            scanf("%d", &priority);
            printf("Enter Call Duration (max %d seconds): ", MAX_CALL_DURATION);
            scanf("%d", &duration);
            printf("Enter Caller Name: ");
            scanf("%s", caller_name);
            printf("Enter Phone Number: ");
            scanf("%s", phone_number);
            enqueue(createCall(priority, duration, caller_name, phone_number));
            printf("Call ID %d added to queue.\n", call_id_counter - 1);
            break;
        case 2:
            assignCall();
            break;
        case 3:
            printf("Enter Agent ID to release: ");
            scanf("%d", &agent_id);
            releaseAgent(agent_id);
            break;
        case 4:
            displayQueue();
            break;
        case 5:
            displayAgentStatus();
            break;
        case 6:
            saveData();
            break;
        case 7:
            saveData();
            pthread_mutex_destroy(&queue_lock);
            free(agents);
            return 0;
        default:
            printf("Invalid choice!\n");
        }
    }
}

