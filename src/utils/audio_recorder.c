#include "utils/audio_recorder.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define WSL_UPLOAD_PORT 8890
#define LOCAL_REC_WAV "/tmp/rec.wav"

typedef struct {
    int duration;
    char wsl_ip[64];
    char wsl_save_path[256];
    unsigned long session_id;
} audio_job_t;

static pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_job_mutex = PTHREAD_MUTEX_INITIALIZER;
static audio_recorder_state_t g_state = AUDIO_RECORDER_IDLE;
static int g_worker_busy = 0;
static int g_stop_requested = 0;
static unsigned long g_session_id = 0;
static audio_job_t g_job;
static audio_recorder_state_cb_t g_state_cb = NULL;

static const char *state_name(audio_recorder_state_t state)
{
    switch (state) {
    case AUDIO_RECORDER_IDLE: return "IDLE";
    case AUDIO_RECORDER_RECORDING: return "RECORDING";
    case AUDIO_RECORDER_SENDING: return "SENDING";
    case AUDIO_RECORDER_WAITING_RESPONSE: return "WAITING_RESPONSE";
    case AUDIO_RECORDER_PLAYING: return "PLAYING";
    case AUDIO_RECORDER_DONE: return "DONE";
    case AUDIO_RECORDER_ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

static void emit_state(audio_recorder_state_t state)
{
    audio_recorder_state_cb_t cb = NULL;
    pthread_mutex_lock(&g_state_mutex);
    g_state = state;
    cb = g_state_cb;
    pthread_mutex_unlock(&g_state_mutex);
    printf("[AudioRecorder] state=%s\n", state_name(state));
    if (cb) cb(state);
}

static unsigned long current_session_locked(void)
{
    return g_session_id;
}

static int session_valid(unsigned long session_id)
{
    int valid;
    pthread_mutex_lock(&g_state_mutex);
    valid = (session_id == g_session_id);
    pthread_mutex_unlock(&g_state_mutex);
    return valid;
}

static void clear_worker_flags(void)
{
    pthread_mutex_lock(&g_state_mutex);
    g_worker_busy = 0;
    g_stop_requested = 0;
    pthread_mutex_unlock(&g_state_mutex);
}

static int record_sync(int duration)
{
    char cmd[512];
    int ret;

    snprintf(cmd, sizeof(cmd), "arecord -D plughw:1,0 -d %d -r 16000 -c 1 -f S16_LE /tmp/rec.wav >/dev/null 2>&1", duration);
    printf("[AudioRecorder] record cmd: %s\n", cmd);
    ret = system(cmd);
    if (ret != 0) {
        snprintf(cmd, sizeof(cmd), "arecord -d %d -r 16000 -c 1 -f S16_LE /tmp/rec.wav >/dev/null 2>&1", duration);
        printf("[AudioRecorder] record fallback cmd: %s\n", cmd);
        ret = system(cmd);
    }
    return ret == 0 ? 0 : -1;
}

static int send_sync(const char *wsl_ip, const char *wsl_save_path)
{
    FILE *fp = NULL;
    int sock = -1;
    struct sockaddr_in addr;
    char buf[4096];
    size_t n;
    long total = 0;

    (void)wsl_save_path;

    printf("[AudioRecorder] tcp upload start file=%s target=%s:%d\n", LOCAL_REC_WAV, wsl_ip, WSL_UPLOAD_PORT);

    fp = fopen(LOCAL_REC_WAV, "rb");
    if (!fp) {
        printf("[AudioRecorder] tcp upload failed: open %s errno=%d\n", LOCAL_REC_WAV, errno);
        return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("[AudioRecorder] tcp upload failed: socket errno=%d\n", errno);
        fclose(fp);
        return -2;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(WSL_UPLOAD_PORT);
    if (inet_pton(AF_INET, wsl_ip, &addr.sin_addr) != 1) {
        printf("[AudioRecorder] tcp upload failed: bad wsl_ip=%s\n", wsl_ip);
        close(sock);
        fclose(fp);
        return -3;
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        printf("[AudioRecorder] tcp upload failed: connect target=%s:%d errno=%d\n", wsl_ip, WSL_UPLOAD_PORT, errno);
        close(sock);
        fclose(fp);
        return -4;
    }

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        size_t sent = 0;
        while (sent < n) {
            ssize_t ret = send(sock, buf + sent, n - sent, 0);
            if (ret <= 0) {
                printf("[AudioRecorder] tcp upload failed: send errno=%d total=%ld\n", errno, total);
                close(sock);
                fclose(fp);
                return -5;
            }
            sent += (size_t)ret;
            total += ret;
        }
    }

    if (ferror(fp)) {
        printf("[AudioRecorder] tcp upload failed: read errno=%d total=%ld\n", errno, total);
        close(sock);
        fclose(fp);
        return -6;
    }

    shutdown(sock, SHUT_WR);
    close(sock);
    fclose(fp);

    printf("[AudioRecorder] tcp upload complete target=%s:%d bytes=%ld\n", wsl_ip, WSL_UPLOAD_PORT, total);
    return 0;
}static void *send_worker_thread(void *arg)
{
    audio_job_t job = *(audio_job_t *)arg;
    free(arg);

    printf("[AudioRecorder] send_worker_thread enter session=%lu state=%s busy=%d\n", job.session_id, state_name(g_state), g_worker_busy);

    if (!session_valid(job.session_id)) {
        printf("[AudioRecorder] send_worker_thread return: invalid session=%lu current=%lu\n", job.session_id, current_session_locked());
        clear_worker_flags();
        return NULL;
    }

    emit_state(AUDIO_RECORDER_SENDING);
    printf("[AudioRecorder] start sending session=%lu state=%s busy=%d\n", job.session_id, state_name(g_state), g_worker_busy);
    if (send_sync(job.wsl_ip, job.wsl_save_path) != 0) {
        printf("[AudioRecorder] send failed session=%lu\n", job.session_id);
        clear_worker_flags();
        if (session_valid(job.session_id)) emit_state(AUDIO_RECORDER_ERROR);
        return NULL;
    }

    printf("[AudioRecorder] send complete session=%lu\n", job.session_id);
    clear_worker_flags();
    if (session_valid(job.session_id)) emit_state(AUDIO_RECORDER_WAITING_RESPONSE);
    return NULL;
}

static void *record_worker_thread(void *arg)
{
    audio_job_t job = *(audio_job_t *)arg;
    free(arg);

    printf("[AudioRecorder] record_worker_thread enter session=%lu state=%s busy=%d\n", job.session_id, state_name(g_state), g_worker_busy);

    if (!session_valid(job.session_id)) {
        printf("[AudioRecorder] record_worker_thread return: invalid session=%lu current=%lu\n", job.session_id, current_session_locked());
        clear_worker_flags();
        return NULL;
    }

    emit_state(AUDIO_RECORDER_RECORDING);
    printf("[AudioRecorder] start recording session=%lu duration=%d state=%s busy=%d\n", job.session_id, job.duration, state_name(g_state), g_worker_busy);
    if (record_sync(job.duration) != 0) {
        int stopped;
        pthread_mutex_lock(&g_state_mutex);
        stopped = g_stop_requested;
        pthread_mutex_unlock(&g_state_mutex);
        printf("[AudioRecorder] record failed session=%lu stopped=%d\n", job.session_id, stopped);
        clear_worker_flags();
        if (session_valid(job.session_id)) emit_state(stopped ? AUDIO_RECORDER_IDLE : AUDIO_RECORDER_ERROR);
        return NULL;
    }

    pthread_mutex_lock(&g_state_mutex);
    if (g_stop_requested) {
        pthread_mutex_unlock(&g_state_mutex);
        printf("[AudioRecorder] record stopped session=%lu\n", job.session_id);
        clear_worker_flags();
        if (session_valid(job.session_id)) emit_state(AUDIO_RECORDER_IDLE);
        return NULL;
    }
    pthread_mutex_unlock(&g_state_mutex);

    audio_job_t *send_job = (audio_job_t *)malloc(sizeof(audio_job_t));
    if (!send_job) {
        printf("[AudioRecorder] alloc send job failed session=%lu\n", job.session_id);
        clear_worker_flags();
        if (session_valid(job.session_id)) emit_state(AUDIO_RECORDER_ERROR);
        return NULL;
    }
    *send_job = job;

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&tid, &attr, send_worker_thread, send_job) != 0) {
        free(send_job);
        printf("[AudioRecorder] start send worker failed session=%lu\n", job.session_id);
        clear_worker_flags();
        if (session_valid(job.session_id)) emit_state(AUDIO_RECORDER_ERROR);
    } else {
        printf("[AudioRecorder] record complete, send worker launched session=%lu\n", job.session_id);
    }
    pthread_attr_destroy(&attr);
    return NULL;
}

void audio_recorder_set_state_callback(audio_recorder_state_cb_t cb)
{
    pthread_mutex_lock(&g_state_mutex);
    g_state_cb = cb;
    pthread_mutex_unlock(&g_state_mutex);
}

audio_recorder_state_t audio_recorder_get_state(void)
{
    audio_recorder_state_t state;
    pthread_mutex_lock(&g_state_mutex);
    state = g_state;
    printf("[AudioRecorder] audio_recorder_get_state enter state=%s session=%lu busy=%d stop=%d\n", state_name(state), g_session_id, g_worker_busy, g_stop_requested);
    pthread_mutex_unlock(&g_state_mutex);
    return state;
}

void audio_recorder_mark_done(void)
{
    audio_recorder_state_cb_t cb = NULL;

    pthread_mutex_lock(&g_state_mutex);
    if (g_state == AUDIO_RECORDER_WAITING_RESPONSE || g_state == AUDIO_RECORDER_PLAYING) {
        g_state = AUDIO_RECORDER_DONE;
        cb = g_state_cb;
    }
    pthread_mutex_unlock(&g_state_mutex);

    if (cb) {
        printf("[AudioRecorder] state=DONE by AI response\n");
        cb(AUDIO_RECORDER_DONE);
    }
}

int audio_recorder_start_record(int duration, const char *wsl_ip, const char *wsl_save_path)
{
    if (duration <= 0 || !wsl_ip || !wsl_ip[0] || !wsl_save_path || !wsl_save_path[0]) {
        printf("[AudioRecorder] audio_recorder_start_record return: bad args duration=%d wsl_ip=%s wsl_save_path=%s\n", duration, wsl_ip ? wsl_ip : "(null)", wsl_save_path ? wsl_save_path : "(null)");
        return -1;
    }

    pthread_mutex_lock(&g_state_mutex);
    if (g_worker_busy || (g_state != AUDIO_RECORDER_IDLE && g_state != AUDIO_RECORDER_ERROR && g_state != AUDIO_RECORDER_DONE)) {
        pthread_mutex_unlock(&g_state_mutex);
        printf("[AudioRecorder] audio_recorder_start_record return: rejected busy=%d state=%s session=%lu\n", g_worker_busy, state_name(g_state), g_session_id);
        return -2;
    }
    g_worker_busy = 1;
    g_stop_requested = 0;
    g_session_id++;
    g_job.session_id = g_session_id;
    pthread_mutex_unlock(&g_state_mutex);

    pthread_mutex_lock(&g_job_mutex);
    g_job.duration = duration;
    strncpy(g_job.wsl_ip, wsl_ip, sizeof(g_job.wsl_ip) - 1);
    g_job.wsl_ip[sizeof(g_job.wsl_ip) - 1] = '\0';
    strncpy(g_job.wsl_save_path, wsl_save_path, sizeof(g_job.wsl_save_path) - 1);
    g_job.wsl_save_path[sizeof(g_job.wsl_save_path) - 1] = '\0';
    pthread_mutex_unlock(&g_job_mutex);

    audio_job_t *job = (audio_job_t *)malloc(sizeof(audio_job_t));
    if (!job) {
        printf("[AudioRecorder] audio_recorder_start_record return: malloc job failed session=%lu\n", g_session_id);
        clear_worker_flags();
        return -3;
    }

    pthread_mutex_lock(&g_job_mutex);
    *job = g_job;
    pthread_mutex_unlock(&g_job_mutex);

    printf("[AudioRecorder] start record accepted session=%lu\n", job->session_id);

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&tid, &attr, record_worker_thread, job) != 0) {
        free(job);
        printf("[AudioRecorder] audio_recorder_start_record return: start record worker failed session=%lu\n", g_session_id);
        clear_worker_flags();
        pthread_attr_destroy(&attr);
        return -4;
    }
    pthread_attr_destroy(&attr);
    return 0;
}

int audio_recorder_stop_record(void)
{
    int need_kill = 0;

    pthread_mutex_lock(&g_state_mutex);
    printf("[AudioRecorder] audio_recorder_stop_record enter state=%s session=%lu busy=%d stop=%d\n", state_name(g_state), g_session_id, g_worker_busy, g_stop_requested);
    if (g_state == AUDIO_RECORDER_RECORDING) {
        g_stop_requested = 1;
        need_kill = 1;
        printf("[AudioRecorder] stop requested session=%lu\n", g_session_id);
    }
    pthread_mutex_unlock(&g_state_mutex);

    if (!need_kill) {
        printf("[AudioRecorder] audio_recorder_stop_record return: not recording state=%s session=%lu\n", state_name(g_state), g_session_id);
        return 0;
    }

    system("killall -9 arecord >/dev/null 2>&1");
    emit_state(AUDIO_RECORDER_IDLE);
    return 0;
}

int audio_recorder_record(int duration)
{
    return record_sync(duration);
}

int audio_recorder_send(const char *wsl_ip, const char *wsl_save_path)
{
    return send_sync(wsl_ip, wsl_save_path);
}

int audio_recorder_record_and_send(int duration, const char *wsl_ip, const char *wsl_save_path)
{
    int ret = audio_recorder_record(duration);
    if (ret != 0) return ret;
    return audio_recorder_send(wsl_ip, wsl_save_path);
}


