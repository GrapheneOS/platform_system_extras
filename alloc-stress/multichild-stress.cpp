#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <cutils/sockets.h>
#include <pthread.h>

#define ONE_MB (1024 * 1024)

/* Child synchronization primitives */
#define STATE_INIT 0
#define STATE_CHILD_READY 1
#define STATE_PARENT_READY 2

struct state_sync {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int state;
};

struct state_sync * init_state_sync_obj() {
    struct state_sync *ssync;

    ssync = (struct state_sync*)mmap(NULL, sizeof(struct state_sync),
                PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (ssync == MAP_FAILED) {
        return NULL;
    }

    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&ssync->mutex, &mattr);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&ssync->condition, &cattr);

    ssync->state = STATE_INIT;
    return ssync;
}

void destroy_state_sync_obj(struct state_sync *ssync) {
    pthread_cond_destroy(&ssync->condition);
    pthread_mutex_destroy(&ssync->mutex);
    munmap(ssync, sizeof(struct state_sync));
}

void signal_state(struct state_sync *ssync, int state) {
    pthread_mutex_lock(&ssync->mutex);
    ssync->state = state;
    pthread_cond_signal(&ssync->condition);
    pthread_mutex_unlock(&ssync->mutex);
}

void wait_for_state(struct state_sync *ssync, int state) {
    pthread_mutex_lock(&ssync->mutex);
    while (ssync->state != state) {
        pthread_cond_wait(&ssync->condition, &ssync->mutex);
    }
    pthread_mutex_unlock(&ssync->mutex);
}

/* LMKD communication */
static int connect_to_lmkd() {
    int sock;
    int tries = 10;
    while ((sock = socket_local_client("lmkd",
                    ANDROID_SOCKET_NAMESPACE_RESERVED,
                    SOCK_SEQPACKET)) < 0) {
        usleep(100000);
        if (tries-- < 0) break;
    }
    if (sock < 0) {
        fprintf(stderr, "Failed to connect to lmkd with err %s\n",
                strerror(errno));
        exit(1);
    }
    return sock;
}

static void write_oomadj_to_lmkd(int sock, uid_t uid, pid_t pid, int oomadj) {
    int written;
    int lmk_procprio_cmd[4];

    lmk_procprio_cmd[0] = htonl(1);
    lmk_procprio_cmd[1] = htonl(pid);
    lmk_procprio_cmd[2] = htonl(uid);
    lmk_procprio_cmd[3] = htonl(oomadj);

    written = write(sock, lmk_procprio_cmd, sizeof(lmk_procprio_cmd));
    if (written == -1) {
        fprintf(stderr, "Failed to send data to lmkd with err %s\n",
                strerror(errno));
        close(sock);
        exit(1);
    }
    printf("Wrote %d bytes to lmkd control socket.\n", written);
}

/* Utility functions */
void set_oom_score(const char *oom_score) {
    int fd, ret;
    fd = open("/proc/self/oom_score_adj", O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Opening oom_score_adj failed with err %s\n",
                strerror(errno));
        exit(1);
    }
    do {
        ret = write(fd, oom_score, strlen(oom_score));
    } while (ret < 0 && errno == EINTR);
    close(fd);
    if (ret < 0) {
        fprintf(stderr, "Writing oom_score_adj failed with err %s\n",
                strerror(errno));
        exit(1);
    }
}

volatile void *gptr;
void add_pressure(size_t *shared, bool *shared_res, size_t total_size, size_t size,
                  size_t duration) {
    volatile void *ptr;
    size_t allocated_size = 0;

    if (total_size == 0)
        total_size = (size_t)-1;

    *shared_res = 0;

    while (allocated_size < total_size) {
        ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
        if (ptr != MAP_FAILED) {
            /* create ptr aliasing to prevent compiler from optimizing the access */
            gptr = ptr;
            /* make data non-zero */
            memset((void*)ptr, (int)(allocated_size + 1), size);
            allocated_size += size;
            *shared = allocated_size;
        }
        usleep(duration);
    }

    *shared_res = (allocated_size >= total_size);
}

static int create_memcg() {
    char buf[256];
    uid_t uid = getuid();
    pid_t pid = getpid();

    snprintf(buf, sizeof(buf), "/dev/memcg/apps/uid_%u", uid);
    int tasks = mkdir(buf, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (tasks < 0 && errno != EEXIST) {
        fprintf(stderr, "Failed to create memory cgroup under %s\n", buf);
        return -1;
    }

    snprintf(buf, sizeof(buf), "/dev/memcg/apps/uid_%u/pid_%u", uid, pid);
    tasks = mkdir(buf, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (tasks < 0) {
        fprintf(stderr, "Failed to create memory cgroup under %s\n", buf);
        return -2;
    }

    snprintf(buf, sizeof(buf), "/dev/memcg/apps/uid_%u/pid_%u/tasks", uid, pid);
    tasks = open(buf, O_WRONLY);
    if (tasks < 0) {
        fprintf(stderr, "Unable to add process to memory cgroup\n");
        return -3;
    }
    snprintf(buf, sizeof(buf), "%u", pid);
    write(tasks, buf, strlen(buf));
    close(tasks);

    snprintf(buf, sizeof(buf), "/dev/memcg/apps/uid_%u/pid_%u/memory.soft_limit_in_bytes", uid, pid);
    tasks = open(buf, O_WRONLY);
    if (tasks < 0) {
        return -4;
    }
    snprintf(buf, sizeof(buf), "%lu", 549755813888);
    write(tasks, buf, strlen(buf));
    close(tasks);

    return 0;
}

/* Child main routine */
void run_child_main(struct state_sync *ssync, bool use_memcg, size_t *shared, bool *shared_res,
                    size_t total_size, size_t size, size_t duration, const char *oom_score) {
    pid_t cpid = getpid();

    if (use_memcg) {
        if (create_memcg() != 0) {
            fprintf(stderr, "Child [pid=%u] failed to create a cgroup\n", cpid);
                *shared = 0;
                *shared_res = false;
            exit(1);
        }
    }
    signal_state(ssync, STATE_CHILD_READY);
    wait_for_state(ssync, STATE_PARENT_READY);

    set_oom_score(oom_score);
    add_pressure(shared, shared_res, total_size, size, duration);
}

/* Parent main routine and utility functions */
void usage()
{
    printf("Application to generate memory pressure by spawning multiple "
           "child processes each allocating memory until being killed\n"
           "Usage: [OPTIONS]\n\n"
           "  -i N: Number of children to spawn.\n"
           "  -d N: Duration in microsecond to sleep between each allocation.\n"
           "  -o N: The oom_score to set the child process to before alloc.\n"
           "  -s N: Number of bytes to allocate in an alloc process loop.\n"
           "  -m N: Number of bytes for each child to allocate, unlimited if omitted.\n"
           "  -g: Create cgroup for each child.\n"
           );
}

long parse_arg(const char *arg, const char *arg_name) {
    char *end;
    long value = strtol(arg, &end, 0);
    if (end == arg || *end != '\0') {
        fprintf(stderr, "Argument %s is not a valid number\n", arg_name);
        exit(1);
    }
    return value;
}

int main(int argc, char *argv[])
{
    pid_t pid;
    size_t *shared;
    bool *shared_res;
    int c, i = 0;

    size_t duration = 1000;
    int iterations = 0;
    const char *oom_score = "899";
    int oom_score_val = 899;
    size_t size = 2 * ONE_MB; // 2 MB
    size_t total_size = 0;
    bool use_memcg = false;
    int sock;
    uid_t uid;
    struct state_sync *ssync;

    while ((c = getopt(argc, argv, "hgi:d:o:s:m:")) != -1) {
        switch (c)
            {
            case 'i':
                iterations = (int)parse_arg(optarg, "-i");
                break;
            case 'd':
                duration = (size_t)parse_arg(optarg, "-i");
                break;
            case 'o':
                oom_score_val = (int)parse_arg(optarg, "-o");
                oom_score = optarg;
                break;
            case 's':
                size = (size_t)parse_arg(optarg, "-s");
                break;
            case 'm':
                total_size = (size_t)parse_arg(optarg, "-m");
                break;
            case 'g':
                use_memcg = true;
                break;
            case 'h':
                usage();
                exit(0);
            default:
                fprintf(stderr, "Invalid argument!");
                exit(1);
            }
    }

    sock = connect_to_lmkd();
    /* uid for parent and children is the same */
    uid = getuid();
    write_oomadj_to_lmkd(sock, uid, getpid(), -1000);

    shared = (size_t*)mmap(NULL, sizeof(size_t) * iterations, PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (shared == MAP_FAILED) {
        fprintf(stderr, "Memory allocation failure!");
        exit(1);
    }
    shared_res = (bool*)mmap(NULL, sizeof(bool) * iterations, PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (shared_res == MAP_FAILED) {
        fprintf(stderr, "Memory allocation failure!");
        exit(1);
    }
    ssync = init_state_sync_obj();
    if (!ssync) {
        fprintf(stderr, "Memory allocation failure!");
        exit(1);
    }

    while (iterations == 0 || i < iterations) {
        pid = fork();
        switch (pid) {
        case (-1):
            fprintf(stderr, "Failed to spawn a child process with err %s\n",
                    strerror(errno));
            exit(1);
            break;
        case (0):
            /* Child */
            run_child_main(ssync, use_memcg, &shared[i], &shared_res[i],
                           total_size, size, duration, oom_score);
            /* might not reach here if child was killed by OOM/LMK */
            exit(0);
            break;
        default:
            /* Parent */
            wait_for_state(ssync, STATE_CHILD_READY);
            write_oomadj_to_lmkd(sock, uid, pid, oom_score_val);
            signal_state(ssync, STATE_PARENT_READY);
            printf("Child %d [pid=%u] started\n", i, pid);
            break;
        }
        i++;
    }

    for (i = 0; i < iterations; i++) {
        pid = wait(NULL);
        printf("Child %d [pid=%u] finished\n", i, pid);
        fflush(stdout);
    }

    for (i = 0; i < iterations; i++) {
        if (shared_res[i]) {
            printf("Child %d allocated %zd MB\n", i, shared[i] / ONE_MB);
        } else {
            printf("Child %d allocated %zd MB before it was killed\n", i, shared[i] / ONE_MB);
        }
    }
    destroy_state_sync_obj(ssync);
    close(sock);
}
