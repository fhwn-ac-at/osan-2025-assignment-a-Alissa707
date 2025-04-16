//
// Created by alissa on 16.04.2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <mqueue.h>
#include <time.h>

#define TASK_QUEUE "/task_queue"
#define RESULT_QUEUE "/result_queue"
#define MAX_MSG_SIZE 128

// Structure for sending worker results back to the ventilator
typedef struct {
    int worker_id;
    pid_t pid;
    int tasks_done;
    int total_sleep_time;
} result_msg_t;

// Prints current time in HH:MM:SS format
void print_timestamp() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("%02d:%02d:%02d | ", t->tm_hour, t->tm_min, t->tm_sec);
}

// Function executed by worker processes
void run_worker(int worker_id) {
    int tasks_processed = 0;
    int total_sleep = 0;
    srand(time(NULL) ^ getpid());

    // Open task queue to receive tasks
    mqd_t task_q = mq_open(TASK_QUEUE, O_RDONLY);
    if (task_q == -1) {
        perror("mq_open task_q");
        exit(EXIT_FAILURE);
    }

    // Open result queue to send completion message
    mqd_t result_q = mq_open(RESULT_QUEUE, O_WRONLY);
    if (result_q == -1) {
        perror("mq_open result_q");
        exit(EXIT_FAILURE);
    }

    while (1) {
        int effort;
        // Wait for a task from the task queue
        if (mq_receive(task_q, (char *)&effort, sizeof(int), NULL) == -1) {
            perror("mq_receive");
            continue;
        }

        // Check for termination signal (effort = 0)
        if (effort == 0) {
            print_timestamp();
            printf("Worker #%02d | Received termination task\n", worker_id);
            fflush(stdout);
            break;
        }

        // Simulate work by sleeping
        print_timestamp();
        printf("Worker #%02d | Received task with effort %d\n", worker_id, effort);
        fflush(stdout);
        sleep(effort);
        tasks_processed++;
        total_sleep += effort;
    }

    // Send final result to ventilator
    result_msg_t result;
    result.worker_id = worker_id;
    result.pid = getpid();
    result.tasks_done = tasks_processed;
    result.total_sleep_time = total_sleep;

    if (mq_send(result_q, (char *)&result, sizeof(result_msg_t), 0) == -1) {
        perror("mq_send result");
    }

    mq_close(task_q);
    mq_close(result_q);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    // If called as a worker process
    if (argc == 3 && strcmp(argv[1], "worker") == 0) {
        int id = atoi(argv[2]);
        run_worker(id);
    }

    // --- VENTILATOR PROCESS STARTS HERE ---
    int opt, workers = 0, tasks = 0, queue_size = 0;

    // Parse command-line arguments
    while ((opt = getopt(argc, argv, "w:t:s:")) != -1) {
        switch (opt) {
            case 'w': workers = atoi(optarg); break;
            case 't': tasks = atoi(optarg); break;
            case 's': queue_size = atoi(optarg); break;
            default:
                fprintf(stderr, "Usage: %s -w <workers> -t <tasks> -s <queue_size>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    print_timestamp();
    printf("Ventilator | Starting %d workers for %d tasks and a queue size of %d\n", workers, tasks, queue_size);
    fflush(stdout);

    // Create and open the task queue
    struct mq_attr attr = {.mq_flags = 0, .mq_maxmsg = queue_size, .mq_msgsize = sizeof(int)};
    mq_unlink(TASK_QUEUE);
    mqd_t task_q = mq_open(TASK_QUEUE, O_CREAT | O_RDWR, 0666, &attr);
    if (task_q == -1) { perror("mq_open task_q"); exit(EXIT_FAILURE); }

    // Create and open the result queue
    int max_results = (workers > 10) ? 10 : workers;

    struct mq_attr r_attr = {
        .mq_flags = 0,
        .mq_maxmsg = max_results,
        .mq_msgsize = sizeof(result_msg_t)
    };
    mq_unlink(RESULT_QUEUE);
    mqd_t result_q = mq_open(RESULT_QUEUE, O_CREAT | O_RDWR, 0666, &r_attr);
    if (result_q == -1) {
      perror("mq_open result_q");
      exit(EXIT_FAILURE);
    }

    pid_t pids[workers];

    // Start worker processes
    for (int i = 0; i < workers; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child replaces itself with a worker call
            char id_str[10];
            sprintf(id_str, "%d", i + 1);
            execl(argv[0], argv[0], "worker", id_str, NULL);
            perror("execl");
            exit(EXIT_FAILURE);
        } else {
            pids[i] = pid;
            print_timestamp();
            printf("Worker #%02d | Started worker PID %d\n", i + 1, pid);
            fflush(stdout);
        }
    }

    sleep(1); // Give workers time to start

    print_timestamp();
    printf("Ventilator | Distributing tasks\n");
    fflush(stdout);

    // Send tasks to the queue
    srand(time(NULL));
    for (int i = 0; i < tasks; ++i) {
        int effort = rand() % 10 + 1;
        print_timestamp();
        printf("Ventilator | Queuing task #%d with effort %d\n", i + 1, effort);
        fflush(stdout);
        if (mq_send(task_q, (char *)&effort, sizeof(int), 0) == -1) {
            perror("mq_send task");
        }
    }

    // Send termination tasks to workers
    print_timestamp();
    printf("Ventilator | Sending termination tasks\n");
    fflush(stdout);
    for (int i = 0; i < workers; ++i) {
        int term = 0;
        if (mq_send(task_q, (char *)&term, sizeof(int), 0) == -1) {
            perror("mq_send termination");
        }
    }

    print_timestamp();
    printf("Ventilator | Waiting for workers to terminate\n");
    fflush(stdout);

    // Collect results from workers and wait for them to finish
    for (int i = 0; i < workers; ++i) {
        result_msg_t res;
        if (mq_receive(result_q, (char *)&res, sizeof(res), NULL) == -1) {
            perror("mq_receive result");
            continue;
        }

        print_timestamp();
        printf("Ventilator | Worker %d processed %d tasks in %d seconds\n",
               res.worker_id, res.tasks_done, res.total_sleep_time);
        fflush(stdout);

        int status;
        if (waitpid(res.pid, &status, 0) > 0) {
            print_timestamp();
            printf("Ventilator | Worker %d with PID %d exited with status %d\n",
                   res.worker_id, res.pid, WEXITSTATUS(status));
            fflush(stdout);
        }
    }

    // close and unlink message queues
    mq_close(task_q);
    mq_unlink(TASK_QUEUE);
    mq_close(result_q);
    mq_unlink(RESULT_QUEUE);

    return 0;
}
